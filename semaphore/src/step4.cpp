// ============================================================
//  PASO 4 — Métodos
//  Objetivo: implementar el comportamiento de cada clase.
//
//  Novedades respecto al paso anterior:
//  • Timer::millis() usa clock_gettime (POSIX)
//  • Led::on()/off() son métodos ABSTRACTOS (virtual puro)
//  • Cada LED imprime un bloque de color ANSI a stdout
//  • Button lee la tecla SPACE sin bloquear (termios raw)
//  • Traffic tiene una FSM básica con countdown
//  • NO hay relación Button→Traffic todavía (eso es paso 5)
// ============================================================

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>

// POSIX / terminal
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

// ============================================================
//  Utilidades de terminal (raw mode + kbhit)
// ============================================================
namespace Terminal {
    static struct termios orig_;
    static bool rawActive_ = false;

    void enableRaw() {
        tcgetattr(STDIN_FILENO, &orig_);
        struct termios raw = orig_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN]  = 0;   // non-blocking
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        rawActive_ = true;
    }

    void disableRaw() {
        if (rawActive_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
            rawActive_ = false;
        }
    }

    // Retorna el carácter disponible en stdin, o 0 si no hay ninguno
    char readKey() {
        char c = 0;
        { ssize_t r=read(STDIN_FILENO,&c,1); (void)r; }
        return c;
    }
}

// Restaurar terminal al salir (Ctrl+C)
static void onSignal(int) {
    Terminal::disableRaw();
    printf("\n[saliendo]\n");
    _exit(0);
}

// ============================================================
//  Timer
// ============================================================
class Timer {
public:
    static uint32_t millis() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
    static void delay(uint32_t ms) {
        uint32_t start = millis();
        while (millis() - start < ms) {}
    }
};

// ============================================================
//  Led — clase base abstracta
// ============================================================
class Led {
protected:
    uint8_t pin_;
    bool    state_;
    const char* label_;   // nombre para el printf

public:
    Led(uint8_t pin, const char* label)
        : pin_(pin), state_(false), label_(label) {}

    virtual ~Led() = default;

    void init() { state_ = false; }

    // Métodos abstractos: cada subclase sabe qué color imprimir
    virtual void on()  = 0;
    virtual void off() = 0;

    bool isOn() const { return state_; }
};

// ============================================================
//  RedLed
// ============================================================
class RedLed : public Led {
public:
    RedLed(uint8_t pin = 1) : Led(pin, "ROJO  ") {}

    void on()  override {
        state_ = true;
        // ANSI: fondo rojo brillante
        printf("\r  \033[41;97m  %s  \033[0m  encendido \n", label_);
        fflush(stdout);
    }
    void off() override {
        state_ = false;
        printf("\r  \033[2m  %s  \033[0m  apagado  \n", label_);
        fflush(stdout);
    }
};

// ============================================================
//  YellowLed
// ============================================================
class YellowLed : public Led {
public:
    YellowLed(uint8_t pin = 2) : Led(pin, "AMARIL") {}

    void on()  override {
        state_ = true;
        printf("\r  \033[43;30m  %s  \033[0m  encendido \n", label_);
        fflush(stdout);
    }
    void off() override {
        state_ = false;
        printf("\r  \033[2m  %s  \033[0m  apagado  \n", label_);
        fflush(stdout);
    }

    // Parpadeo no bloqueante: invierte el estado si pasó periodMs
    void blink(uint32_t now, uint32_t periodMs) {
        static uint32_t lastToggle = 0;
        if (now - lastToggle >= periodMs) {
            lastToggle = now;
            if (state_) off(); else on();
        }
    }
};

// ============================================================
//  GreenLed
// ============================================================
class GreenLed : public Led {
public:
    GreenLed(uint8_t pin = 3) : Led(pin, "VERDE ") {}

    void on()  override {
        state_ = true;
        printf("\r  \033[42;30m  %s  \033[0m  encendido \n", label_);
        fflush(stdout);
    }
    void off() override {
        state_ = false;
        printf("\r  \033[2m  %s  \033[0m  apagado  \n", label_);
        fflush(stdout);
    }
};

// ============================================================
//  Button
// ============================================================
class Button {
    uint8_t pin_;
    bool    state_;
    bool    requested_;

public:
    explicit Button(uint8_t pin = 0)
        : pin_(pin), state_(false), requested_(false) {}

    void init() { state_ = false; requested_ = false; }

    // Llamar cada tick: lee teclado, registra SPACE
    void update() {
        char c = Terminal::readKey();
        bool pressed = (c == ' ');
        state_ = pressed;
        if (pressed && !requested_) {
            requested_ = true;
            printf("\n  [BOTÓN] Solicitud de cruce registrada!\n");
            fflush(stdout);
        }
    }

    bool isRequested() const { return requested_; }

private:
    friend class Traffic;   // Traffic puede llamar clearRequest
    void clearRequest() { requested_ = false; }
};

// ============================================================
//  Traffic — FSM del semáforo
//  (En paso 4 todavía NO está conectado al Button)
// ============================================================
class Traffic {
public:
    static constexpr uint32_t GREEN_TIME  = 5000;
    static constexpr uint32_t YELLOW_TIME = 2000;
    static constexpr uint32_t RED_TIME    = 5000;
    static constexpr uint32_t BLINKY_TIME =  300;

    enum State : uint8_t {
        ST_GREEN = 0,
        ST_YELLOW,
        ST_RED,
        ST_BLINKY   // amarillo parpadeante antes de volver a verde
    };

private:
    uint8_t  currentState_;
    uint32_t stateTime_;
    uint32_t lastCountdown_;  // última vez que imprimimos countdown

    RedLed    red_;
    YellowLed yellow_;
    GreenLed  green_;

    // ── Helpers ────────────────────────────────────────────
    void allOff() {
        red_.off(); yellow_.off(); green_.off();
    }

    void enterState(uint8_t s) {
        currentState_ = s;
        stateTime_    = Timer::millis();
        lastCountdown_ = stateTime_;
        allOff();
        switch (s) {
            case ST_GREEN:  printf("\n--- VERDE  (peatones: esperen) ---\n"); green_.on();  break;
            case ST_YELLOW: printf("\n--- AMARILLO (precaución) ---\n");      yellow_.on(); break;
            case ST_RED:    printf("\n--- ROJO   (peatones: crucen) ---\n");  red_.on();    break;
            case ST_BLINKY: printf("\n--- AMARILLO BLINK (fin cruce) ---\n");              break;
        }
        fflush(stdout);
    }

    // Tiempo total del estado actual
    uint32_t stateTimeout() const {
        switch (currentState_) {
            case ST_GREEN:  return GREEN_TIME;
            case ST_YELLOW: return YELLOW_TIME;
            case ST_RED:    return RED_TIME;
            case ST_BLINKY: return YELLOW_TIME;
            default:        return 1000;
        }
    }

    const char* stateName() const {
        switch (currentState_) {
            case ST_GREEN:  return "VERDE  ";
            case ST_YELLOW: return "AMARIL.";
            case ST_RED:    return "ROJO   ";
            case ST_BLINKY: return "BLINKY ";
            default:        return "???    ";
        }
    }

public:
    Traffic() : currentState_(ST_GREEN), stateTime_(0), lastCountdown_(0) {}

    void init() { enterState(ST_GREEN); }

    void update() {
        uint32_t now     = Timer::millis();
        uint32_t elapsed = now - stateTime_;
        uint32_t timeout = stateTimeout();

        // ── Countdown cada 500 ms ───────────────────────────
        if (now - lastCountdown_ >= 500) {
            lastCountdown_ = now;
            uint32_t remaining = (elapsed < timeout) ? (timeout - elapsed) : 0;
            printf("  [%s] %u.%u s restantes\n",
                   stateName(),
                   remaining / 1000,
                   (remaining % 1000) / 100);
            fflush(stdout);
        }

        // ── Parpadeo amarillo ───────────────────────────────
        if (currentState_ == ST_BLINKY) {
            yellow_.blink(now, BLINKY_TIME);
        }

        // ── Transición de estado ────────────────────────────
        if (elapsed >= timeout) {
            switch (currentState_) {
                case ST_GREEN:  enterState(ST_YELLOW); break;
                case ST_YELLOW: enterState(ST_RED);    break;
                case ST_RED:    enterState(ST_BLINKY); break;
                case ST_BLINKY: enterState(ST_GREEN);  break;
            }
        }
    }

    // (En paso 4 no usado aún, se conecta en paso 5)
    void requestCross() {}
};

// ============================================================
//  App
// ============================================================
class App {
    Button  button_;
    Traffic traffic_;

public:
    void init() {
        printf("╔══════════════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 4             ║\n");
        printf("║   Métodos implementados                  ║\n");
        printf("║   LEDs simulados por colores ANSI        ║\n");
        printf("║   Presioná SPACE para solicitar cruce    ║\n");
        printf("║   (el botón aún no actúa sobre el FSM)   ║\n");
        printf("║   Ctrl+C para salir                      ║\n");
        printf("╚══════════════════════════════════════════╝\n\n");
        button_.init();
        traffic_.init();
    }

    void run() {
        init();
        while (true) {
            button_.update();   // lee teclado, imprime si hay press
            traffic_.update();  // avanza FSM + countdown
            Timer::delay(50);
        }
    }
};

// ============================================================
//  main
// ============================================================
int main() {
    signal(SIGINT,  onSignal);
    signal(SIGTERM, onSignal);
    Terminal::enableRaw();

    App app;
    app.run();   // nunca retorna

    Terminal::disableRaw();
    return 0;
}
