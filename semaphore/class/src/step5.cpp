// ============================================================
//  PASO 5 — Relaciones
//  Objetivo: cablear las relaciones del diagrama UML.
//
//  Novedades respecto al paso anterior:
//  • Traffic recibe una referencia a Button en su constructor
//    (asociación UML  Traffic --> Button)
//  • Traffic::update() consulta button_.isRequested() y llama
//    requestCross() cuando hay una solicitud pendiente
//  • Al cruzar: el ciclo se fuerza a ST_YELLOW → ST_RED
//  • App coordina la composición: posee Button y Traffic,
//    y pasa la referencia de button_ a traffic_
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

// ============================================================
//  Terminal (igual que pasos anteriores)
// ============================================================
namespace Terminal {
    static struct termios orig_;
    static bool rawActive_ = false;
    void enableRaw() {
        tcgetattr(STDIN_FILENO, &orig_);
        struct termios raw = orig_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        rawActive_ = true;
    }
    void disableRaw() {
        if (rawActive_) { tcsetattr(STDIN_FILENO, TCSANOW, &orig_); rawActive_ = false; }
    }
    char readKey() { char c = 0; { ssize_t r=read(STDIN_FILENO,&c,1); (void)r; } return c; }
}
static void onSignal(int) { Terminal::disableRaw(); printf("\n[saliendo]\n"); _exit(0); }

// ============================================================
//  Timer
// ============================================================
class Timer {
public:
    static uint32_t millis() {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
    static void delay(uint32_t ms) { uint32_t s = millis(); while (millis()-s < ms) {} }
};

// ============================================================
//  Led (abstracta) + subclases
// ============================================================
class Led {
protected:
    uint8_t pin_; bool state_; const char* label_;
public:
    Led(uint8_t pin, const char* label) : pin_(pin), state_(false), label_(label) {}
    virtual ~Led() = default;
    void init() { state_ = false; }
    virtual void on()  = 0;
    virtual void off() = 0;
    bool isOn() const { return state_; }
};

class RedLed : public Led {
public:
    RedLed(uint8_t pin=1) : Led(pin,"ROJO  ") {}
    void on()  override { state_=true;  printf("\r  \033[41;97m  %s  \033[0m  ON \n",label_); fflush(stdout); }
    void off() override { state_=false; printf("\r  \033[2m  %s  \033[0m  off\n",label_); fflush(stdout); }
};

class YellowLed : public Led {
public:
    YellowLed(uint8_t pin=2) : Led(pin,"AMARIL") {}
    void on()  override { state_=true;  printf("\r  \033[43;30m  %s  \033[0m  ON \n",label_); fflush(stdout); }
    void off() override { state_=false; printf("\r  \033[2m  %s  \033[0m  off\n",label_); fflush(stdout); }
    void blink(uint32_t now, uint32_t period) {
        static uint32_t last=0;
        if (now-last >= period) { last=now; if(state_) off(); else on(); }
    }
};

class GreenLed : public Led {
public:
    GreenLed(uint8_t pin=3) : Led(pin,"VERDE ") {}
    void on()  override { state_=true;  printf("\r  \033[42;30m  %s  \033[0m  ON \n",label_); fflush(stdout); }
    void off() override { state_=false; printf("\r  \033[2m  %s  \033[0m  off\n",label_); fflush(stdout); }
};

// ============================================================
//  Button
// ============================================================
class Button {
    uint8_t pin_; bool state_; bool requested_;
public:
    explicit Button(uint8_t pin=0) : pin_(pin), state_(false), requested_(false) {}
    void init()  { state_=false; requested_=false; }
    void update() {
        char c = Terminal::readKey();
        bool pressed = (c == ' ');
        state_ = pressed;
        if (pressed && !requested_) {
            requested_ = true;
            printf("\n  \033[36m[BOTÓN] Solicitud de cruce registrada!\033[0m\n");
            fflush(stdout);
        }
    }
    bool isRequested() const { return requested_; }
    void clearRequest() { requested_ = false; }  // público: Traffic lo llama
};

// ============================================================
//  Traffic — FSM conectada al Button (ASOCIACIÓN real)
// ============================================================
class Traffic {
public:
    static constexpr uint32_t GREEN_TIME  = 5000;
    static constexpr uint32_t YELLOW_TIME = 2000;
    static constexpr uint32_t RED_TIME    = 5000;
    static constexpr uint32_t BLINKY_TIME =  300;

    enum State : uint8_t { ST_GREEN=0, ST_YELLOW, ST_RED, ST_BLINKY };

private:
    uint8_t  currentState_;
    uint32_t stateTime_;
    uint32_t lastCountdown_;
    bool     crossRequested_;  // flag interno: peatón espera

    RedLed    red_;
    YellowLed yellow_;
    GreenLed  green_;

    // ── Asociación: puntero al Button que App nos pasó ──
    Button* button_;           // Traffic --> Button

    void allOff() { red_.off(); yellow_.off(); green_.off(); }

    void enterState(uint8_t s) {
        currentState_ = s; stateTime_ = Timer::millis(); lastCountdown_ = stateTime_;
        allOff();
        switch(s) {
            case ST_GREEN:
                printf("\n\033[32m--- VERDE  --- (peatones: esperen)\033[0m\n");
                if (crossRequested_)
                    printf("  \033[33m[aviso] hay solicitud pendiente, se atenderá al próximo ciclo\033[0m\n");
                green_.on(); break;
            case ST_YELLOW:
                printf("\n\033[33m--- AMARILLO --- (precaución)\033[0m\n");
                yellow_.on(); break;
            case ST_RED:
                printf("\n\033[31m--- ROJO --- (peatones: ¡crucen!)\033[0m\n");
                red_.on(); break;
            case ST_BLINKY:
                printf("\n\033[33m--- AMARILLO BLINK --- (fin de cruce)\033[0m\n"); break;
        }
        fflush(stdout);
    }

    uint32_t stateTimeout() const {
        switch(currentState_) {
            case ST_GREEN:  return crossRequested_ ? GREEN_TIME/2 : GREEN_TIME;
            case ST_YELLOW: return YELLOW_TIME;
            case ST_RED:    return RED_TIME;
            case ST_BLINKY: return YELLOW_TIME;
            default:        return 1000;
        }
    }

    const char* stateName() const {
        switch(currentState_) {
            case ST_GREEN:  return "VERDE  ";
            case ST_YELLOW: return "AMARIL.";
            case ST_RED:    return "ROJO   ";
            case ST_BLINKY: return "BLINKY ";
            default:        return "???    ";
        }
    }

public:
    // Constructor recibe referencia al Button (asociación UML)
    explicit Traffic(Button& btn)
        : currentState_(ST_GREEN), stateTime_(0), lastCountdown_(0),
          crossRequested_(false), button_(&btn) {}

    void init() { enterState(ST_GREEN); }

    // Llamado por App cuando button_.isRequested()
    void requestCross() {
        if (!crossRequested_) {
            crossRequested_ = true;
            printf("  \033[36m[Traffic] cruce aceptado — se ejecutará en el próximo rojo\033[0m\n");
            fflush(stdout);
        }
        button_->clearRequest();
    }

    void update() {
        uint32_t now     = Timer::millis();
        uint32_t elapsed = now - stateTime_;
        uint32_t timeout = stateTimeout();

        // ── Consulta al Button (asociación activa) ──────────
        if (button_->isRequested()) requestCross();

        // ── Countdown cada 500 ms ───────────────────────────
        if (now - lastCountdown_ >= 500) {
            lastCountdown_ = now;
            uint32_t rem = (elapsed < timeout) ? timeout - elapsed : 0;
            const char* req = crossRequested_ ? " \033[36m[cruce solicitado]\033[0m" : "";
            printf("  [%s] %u.%u s restantes%s\n",
                   stateName(), rem/1000, (rem%1000)/100, req);
            fflush(stdout);
        }

        // ── Parpadeo amarillo ───────────────────────────────
        if (currentState_ == ST_BLINKY) yellow_.blink(now, BLINKY_TIME);

        // ── Transiciones ────────────────────────────────────
        if (elapsed >= timeout) {
            switch(currentState_) {
                case ST_GREEN:  enterState(ST_YELLOW); break;
                case ST_YELLOW: enterState(ST_RED);    break;
                case ST_RED:
                    crossRequested_ = false;  // solicitud atendida
                    enterState(ST_BLINKY);
                    break;
                case ST_BLINKY: enterState(ST_GREEN);  break;
            }
        }
    }
};

// ============================================================
//  App — composición + cableado de relaciones
// ============================================================
class App {
    Button  button_;   // App *-- Button  (composición por valor)
    Traffic traffic_;  // App *-- Traffic (composición por valor)
                       // traffic_ recibe &button_ en su constructor
                       // → relación Traffic --> Button establecida

public:
    App() : button_(0), traffic_(button_) {}

    void init() {
        printf("╔══════════════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 5             ║\n");
        printf("║   Relaciones cableadas                   ║\n");
        printf("║   Traffic consulta Button::isRequested() ║\n");
        printf("║   Presioná SPACE para solicitar cruce    ║\n");
        printf("║   Ctrl+C para salir                      ║\n");
        printf("╚══════════════════════════════════════════╝\n\n");
        button_.init();
        traffic_.init();
    }

    void run() {
        init();
        while (true) {
            button_.update();
            traffic_.update();
            Timer::delay(50);
        }
    }
};

// ============================================================
//  main
// ============================================================
int main() {
    signal(SIGINT, onSignal); signal(SIGTERM, onSignal);
    Terminal::enableRaw();
    App app; app.run();
    Terminal::disableRaw();
    return 0;
}
