// ============================================================
//  PASO 6 — Notas / Decisiones de diseño
//  Objetivo: documentar en el código las mismas decisiones
//  que aparecen como "note" en el diagrama UML.
//
//  Las notas del diagrama se traducen aquí en:
//  • Comentarios de bloque sobre cada clase
//  • static_assert para validar invariantes en compile-time
//  • Banner de arranque que imprime la arquitectura
//
//  Comportamiento en ejecución: idéntico al paso 5.
// ============================================================

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <termios.h>
#include <unistd.h>
#include <csignal>

// ============================================================
//  Terminal
// ============================================================
namespace Terminal {
    static struct termios orig_; static bool rawActive_=false;
    void enableRaw()  { tcgetattr(STDIN_FILENO,&orig_); struct termios r=orig_; r.c_lflag&=~(ICANON|ECHO); r.c_cc[VMIN]=0; r.c_cc[VTIME]=0; tcsetattr(STDIN_FILENO,TCSANOW,&r); rawActive_=true; }
    void disableRaw() { if(rawActive_){tcsetattr(STDIN_FILENO,TCSANOW,&orig_);rawActive_=false;} }
    char readKey()    { char c=0; { ssize_t r=read(STDIN_FILENO,&c,1); (void)r; } return c; }
}
static void onSignal(int){ Terminal::disableRaw(); printf("\n[saliendo]\n"); _exit(0); }

// ============================================================
//  Timer
//  NOTE (del diagrama): "Todos los métodos son static.
//  En hardware real: reemplazar con HAL_GetTick()/HAL_Delay()"
// ============================================================
class Timer {
public:
    static uint32_t millis() {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
        return (uint32_t)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
    }
    static void delay(uint32_t ms) { uint32_t s=millis(); while(millis()-s<ms){} }
};

// ============================================================
//  Led — clase abstracta
//  NOTE (del diagrama): "Clase abstracta: no se puede
//  instanciar. Define la interfaz común on()/off().
//  pin_ es protected para que las subclases accedan sin getter."
// ============================================================
class Led {
protected:
    // NOTE: protected → RedLed/YellowLed/GreenLed leen pin_
    // directamente en sus implementaciones de on()/off()
    // sin necesidad de un getter público
    uint8_t pin_;
    bool    state_;
    const char* label_;
public:
    Led(uint8_t pin, const char* label) : pin_(pin), state_(false), label_(label) {}
    virtual ~Led() = default;
    void init() { state_=false; }
    virtual void on()  = 0;   // abstracto: subclase provee color ANSI
    virtual void off() = 0;   // abstracto: subclase provee color ANSI
    bool isOn() const { return state_; }
};

// ============================================================
//  Subclases concretas de Led
// ============================================================
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
    // NOTE (del diagrama): "Única subclase con blink():
    // la fase de precaución del semáforo la usa exclusivamente."
    void blink(uint32_t now, uint32_t period) {
        static uint32_t last=0;
        if (now-last>=period){ last=now; if(state_) off(); else on(); }
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
//  NOTE (del diagrama): "clearRequest() es llamado
//  internamente por Traffic tras atender la solicitud."
// ============================================================
class Button {
    uint8_t pin_; bool state_; bool requested_;
public:
    explicit Button(uint8_t pin=0): pin_(pin), state_(false), requested_(false) {}
    void init()  { state_=false; requested_=false; }
    void update() {
        char c=Terminal::readKey(); bool pressed=(c==' '); state_=pressed;
        if(pressed && !requested_){
            requested_=true;
            printf("\n  \033[36m[BOTÓN] Solicitud de cruce registrada!\033[0m\n");
            fflush(stdout);
        }
    }
    bool isRequested() const { return requested_; }
    // NOTE: clearRequest() es public para que Traffic lo llame
    // directamente como parte de la asociación Traffic --> Button
    void clearRequest() { requested_=false; }
};

// ============================================================
//  Traffic
//  NOTE (del diagrama super-loop):
//  "1. button.update()  2. traffic.update()"
// ============================================================
class Traffic {
public:
    static constexpr uint32_t GREEN_TIME  = 5000;
    static constexpr uint32_t YELLOW_TIME = 2000;
    static constexpr uint32_t RED_TIME    = 5000;
    static constexpr uint32_t BLINKY_TIME =  300;

    // static_assert: invariante de diseño documentada en compile-time
    static_assert(BLINKY_TIME < YELLOW_TIME, "BLINKY_TIME debe ser menor que YELLOW_TIME");

    enum State : uint8_t { ST_GREEN=0, ST_YELLOW, ST_RED, ST_BLINKY };

private:
    uint8_t  currentState_; uint32_t stateTime_; uint32_t lastCountdown_;
    bool     crossRequested_;
    RedLed    red_; YellowLed yellow_; GreenLed green_;
    Button*   button_;  // asociación (puntero, no posesión)

    void allOff() { red_.off(); yellow_.off(); green_.off(); }

    void enterState(uint8_t s){
        currentState_=s; stateTime_=Timer::millis(); lastCountdown_=stateTime_; allOff();
        switch(s){
            case ST_GREEN:  printf("\n\033[32m--- VERDE  --- (peatones: esperen)\033[0m\n");      green_.on(); break;
            case ST_YELLOW: printf("\n\033[33m--- AMARILLO --- (precaución)\033[0m\n");            yellow_.on(); break;
            case ST_RED:    printf("\n\033[31m--- ROJO --- (peatones: ¡crucen!)\033[0m\n");        red_.on(); break;
            case ST_BLINKY: printf("\n\033[33m--- AMARILLO BLINK --- (fin de cruce)\033[0m\n");   break;
        }
        fflush(stdout);
    }

    uint32_t stateTimeout() const {
        switch(currentState_){
            case ST_GREEN:  return crossRequested_ ? GREEN_TIME/2 : GREEN_TIME;
            case ST_YELLOW: return YELLOW_TIME; case ST_RED: return RED_TIME;
            case ST_BLINKY: return YELLOW_TIME; default: return 1000;
        }
    }
    const char* stateName() const {
        switch(currentState_){
            case ST_GREEN: return "VERDE  "; case ST_YELLOW: return "AMARIL.";
            case ST_RED:   return "ROJO   "; case ST_BLINKY: return "BLINKY "; default: return "???";
        }
    }

public:
    explicit Traffic(Button& btn)
        : currentState_(ST_GREEN), stateTime_(0), lastCountdown_(0),
          crossRequested_(false), button_(&btn) {}

    void init() { enterState(ST_GREEN); }

    void requestCross(){
        if(!crossRequested_){
            crossRequested_=true;
            printf("  \033[36m[Traffic] cruce aceptado\033[0m\n"); fflush(stdout);
        }
        button_->clearRequest();  // NOTE: aquí se llama clearRequest()
    }

    void update(){
        uint32_t now=Timer::millis(), elapsed=now-stateTime_, timeout=stateTimeout();
        if(button_->isRequested()) requestCross();
        if(now-lastCountdown_>=500){
            lastCountdown_=now;
            uint32_t rem=(elapsed<timeout)?timeout-elapsed:0;
            const char* req=crossRequested_?" \033[36m[cruce pendiente]\033[0m":"";
            printf("  [%s] %u.%u s%s\n", stateName(), rem/1000, (rem%1000)/100, req);
            fflush(stdout);
        }
        if(currentState_==ST_BLINKY) yellow_.blink(now, BLINKY_TIME);
        if(elapsed>=timeout){
            switch(currentState_){
                case ST_GREEN:  enterState(ST_YELLOW); break;
                case ST_YELLOW: enterState(ST_RED);    break;
                case ST_RED:    crossRequested_=false; enterState(ST_BLINKY); break;
                case ST_BLINKY: enterState(ST_GREEN);  break;
            }
        }
    }
};

// ============================================================
//  App
// ============================================================
class App {
    Button  button_;
    Traffic traffic_;
public:
    App() : button_(0), traffic_(button_) {}

    void init(){
        printf("╔══════════════════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 6                 ║\n");
        printf("║   Decisiones de diseño documentadas          ║\n");
        printf("╠══════════════════════════════════════════════╣\n");
        printf("║  Timer     → métodos 100%% static             ║\n");
        printf("║  Led       → abstracta, pin_ protected       ║\n");
        printf("║  YellowLed → única con blink()               ║\n");
        printf("║  Button    → clearRequest() llamado por      ║\n");
        printf("║              Traffic tras atender solicitud  ║\n");
        printf("╠══════════════════════════════════════════════╣\n");
        printf("║  Super-loop:                                 ║\n");
        printf("║    1. button.update()                        ║\n");
        printf("║    2. traffic.update() → isRequested()       ║\n");
        printf("╠══════════════════════════════════════════════╣\n");
        printf("║  Presioná SPACE para solicitar cruce         ║\n");
        printf("║  Ctrl+C para salir                           ║\n");
        printf("╚══════════════════════════════════════════════╝\n\n");
        button_.init(); traffic_.init();
    }

    void run(){ init(); while(true){ button_.update(); traffic_.update(); Timer::delay(50); } }
};

int main(){
    signal(SIGINT,onSignal); signal(SIGTERM,onSignal);
    Terminal::enableRaw();
    App app; app.run();
    Terminal::disableRaw(); return 0;
}
