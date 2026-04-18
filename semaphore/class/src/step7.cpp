// ============================================================
//  PASO 7 — Diagrama final
//  Objetivo: presentación profesional completa.
//
//  Equivalencias del skinparam/package UML en C++:
//  • package "HAL"         → namespace HAL { ... }
//  • package "Control"     → namespace Control { ... }
//  • package "Application" → namespace Application { ... }
//  • <<hardware>>          → comentario // [hardware]
//  • <<control>>           → comentario // [control]
//  • <<utility>>           → comentario // [utility]
//
//  Mejoras de presentación:
//  • Pantalla limpia con ANSI (clear screen)
//  • Panel de estado visual: los 3 LEDs dibujados en columna
//  • Countdown en el panel, no mezclado con los logs
//  • Indicador de solicitud de cruce pendiente
// ============================================================

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <csignal>

// ============================================================
//  Terminal utilities
// ============================================================
namespace Terminal {
    static struct termios orig_; static bool rawActive_=false;
    void enableRaw(){
        tcgetattr(STDIN_FILENO,&orig_); struct termios r=orig_;
        r.c_lflag&=~(ICANON|ECHO); r.c_cc[VMIN]=0; r.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO,TCSANOW,&r); rawActive_=true;
    }
    void disableRaw(){ if(rawActive_){tcsetattr(STDIN_FILENO,TCSANOW,&orig_);rawActive_=false;} }
    char readKey(){ char c=0; { ssize_t r=read(STDIN_FILENO,&c,1); (void)r; } return c; }
    void clearScreen(){ printf("\033[2J\033[H"); }
    void moveTo(int row, int col){ printf("\033[%d;%dH", row, col); }
}
static void onSignal(int){ Terminal::disableRaw(); printf("\033[0m\033[2J\033[H\n[saliendo]\n"); _exit(0); }

// ============================================================
//  namespace HAL  (package "HAL" del diagrama)
// ============================================================
namespace HAL {

// ----------------------------------------------------------
// Timer  [utility]
// "Todos los métodos son static.
//  En hardware real: HAL_GetTick() / HAL_Delay()"
// ----------------------------------------------------------
class Timer {
public:
    static uint32_t millis(){
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
        return (uint32_t)(ts.tv_sec*1000+ts.tv_nsec/1000000);
    }
    static void delay(uint32_t ms){ uint32_t s=millis(); while(millis()-s<ms){} }
};

// ----------------------------------------------------------
// Led  [hardware] — clase abstracta
// "Define la interfaz común on()/off().
//  pin_ es protected para acceso directo en subclases."
// ----------------------------------------------------------
class Led {
protected:
    uint8_t     pin_;
    bool        state_;
    const char* ansiOn_;   // código ANSI color encendido
    const char* name_;     // etiqueta de display
public:
    Led(uint8_t pin, const char* ansiOn, const char* name)
        : pin_(pin), state_(false), ansiOn_(ansiOn), name_(name) {}
    virtual ~Led() = default;
    void init() { state_=false; }
    virtual void on()  = 0;
    virtual void off() = 0;
    bool isOn() const { return state_; }
    const char* getName() const { return name_; }
    const char* getAnsiOn() const { return ansiOn_; }
};

// ----------------------------------------------------------
// RedLed  [hardware]
// ----------------------------------------------------------
class RedLed : public Led {
public:
    RedLed(uint8_t pin=1) : Led(pin,"\033[41;97m","ROJO  ") {}
    void on()  override { state_=true; }
    void off() override { state_=false; }
};

// ----------------------------------------------------------
// YellowLed  [hardware]
// "Única subclase con blink(): usada en fase de precaución."
// ----------------------------------------------------------
class YellowLed : public Led {
    uint32_t lastBlink_=0;
public:
    YellowLed(uint8_t pin=2) : Led(pin,"\033[43;30m","AMARIL") {}
    void on()  override { state_=true; }
    void off() override { state_=false; }
    void blink(uint32_t now, uint32_t period){
        if(now-lastBlink_>=period){ lastBlink_=now; if(state_) off(); else on(); }
    }
};

// ----------------------------------------------------------
// GreenLed  [hardware]
// ----------------------------------------------------------
class GreenLed : public Led {
public:
    GreenLed(uint8_t pin=3) : Led(pin,"\033[42;30m","VERDE ") {}
    void on()  override { state_=true; }
    void off() override { state_=false; }
};

// ----------------------------------------------------------
// Button  [hardware]
// "clearRequest() llamado por Traffic tras atender solicitud."
// ----------------------------------------------------------
class Button {
    uint8_t pin_; bool state_; bool requested_;
public:
    explicit Button(uint8_t pin=0) : pin_(pin),state_(false),requested_(false){}
    void init()  { state_=false; requested_=false; }
    void update(){
        char c=Terminal::readKey(); state_=(c==' ');
        if(state_&&!requested_) requested_=true;
    }
    bool isRequested() const { return requested_; }
    void clearRequest()      { requested_=false; }
};

} // namespace HAL

// ============================================================
//  namespace Control  (package "Control" del diagrama)
// ============================================================
namespace Control {

// ----------------------------------------------------------
// Traffic  [control]
// ----------------------------------------------------------
class Traffic {
public:
    static constexpr uint32_t GREEN_TIME  = 5000;
    static constexpr uint32_t YELLOW_TIME = 2000;
    static constexpr uint32_t RED_TIME    = 5000;
    static constexpr uint32_t BLINKY_TIME =  300;
    static_assert(BLINKY_TIME < YELLOW_TIME, "invariante de tiempo");

    enum State : uint8_t { ST_GREEN=0, ST_YELLOW, ST_RED, ST_BLINKY };

    // Estado para el panel de display (leído por App)
    struct DisplayInfo {
        const char* stateName;
        uint32_t    remainingMs;
        bool        crossRequested;
        bool        redOn, yellowOn, greenOn, yellowBlink;
    };

private:
    uint8_t  currentState_;
    uint32_t stateTime_;
    bool     crossRequested_;
    bool     newEvent_;          // true cuando hay algo que loguear
    char     eventMsg_[128];

    HAL::RedLed    red_;
    HAL::YellowLed yellow_;
    HAL::GreenLed  green_;
    HAL::Button*   button_;

    void allOff(){ red_.off(); yellow_.off(); green_.off(); }

    void enterState(uint8_t s){
        currentState_=s; stateTime_=HAL::Timer::millis(); allOff();
        switch(s){
            case ST_GREEN:
                snprintf(eventMsg_,sizeof(eventMsg_),
                    "\033[32m→ VERDE: coches circulan, peatones esperan\033[0m");
                green_.on(); break;
            case ST_YELLOW:
                snprintf(eventMsg_,sizeof(eventMsg_),
                    "\033[33m→ AMARILLO: precaución, preparar parada\033[0m");
                yellow_.on(); break;
            case ST_RED:
                snprintf(eventMsg_,sizeof(eventMsg_),
                    "\033[31m→ ROJO: coches paran, peatones ¡crucen!\033[0m");
                red_.on(); break;
            case ST_BLINKY:
                snprintf(eventMsg_,sizeof(eventMsg_),
                    "\033[33m→ BLINK: fin de cruce, vuelve a verde\033[0m");
                break;
        }
        newEvent_=true;
    }

    uint32_t stateTimeout() const {
        switch(currentState_){
            case ST_GREEN:  return crossRequested_ ? GREEN_TIME/2 : GREEN_TIME;
            case ST_YELLOW: return YELLOW_TIME;
            case ST_RED:    return RED_TIME;
            case ST_BLINKY: return YELLOW_TIME;
            default:        return 1000;
        }
    }

public:
    explicit Traffic(HAL::Button& btn)
        : currentState_(ST_GREEN), stateTime_(0), crossRequested_(false),
          newEvent_(false), button_(&btn) { eventMsg_[0]='\0'; }

    void init(){ enterState(ST_GREEN); }

    void requestCross(){
        if(!crossRequested_){
            crossRequested_=true;
            snprintf(eventMsg_,sizeof(eventMsg_),
                "\033[36m✓ Solicitud de cruce registrada\033[0m");
            newEvent_=true;
        }
        button_->clearRequest();
    }

    void update(){
        uint32_t now=HAL::Timer::millis();
        uint32_t elapsed=now-stateTime_;
        uint32_t timeout=stateTimeout();
        if(button_->isRequested()) requestCross();
        if(currentState_==ST_BLINKY) yellow_.blink(now,BLINKY_TIME);
        if(elapsed>=timeout){
            switch(currentState_){
                case ST_GREEN:  enterState(ST_YELLOW); break;
                case ST_YELLOW: enterState(ST_RED);    break;
                case ST_RED:    crossRequested_=false; enterState(ST_BLINKY); break;
                case ST_BLINKY: enterState(ST_GREEN);  break;
            }
        }
    }

    DisplayInfo getDisplay() const {
        uint32_t now=HAL::Timer::millis();
        uint32_t elapsed=now-stateTime_;
        uint32_t timeout=stateTimeout();
        uint32_t rem=(elapsed<timeout)?timeout-elapsed:0;
        static const char* names[]={"VERDE  ","AMARIL.","ROJO   ","BLINKY "};
        return { names[currentState_], rem,
                 crossRequested_,
                 red_.isOn(), yellow_.isOn(), green_.isOn(),
                 currentState_==ST_BLINKY };
    }

    bool hasEvent()   const { return newEvent_; }
    const char* popEvent()  { newEvent_=false; return eventMsg_; }
};

} // namespace Control

// ============================================================
//  namespace Application  (package "Application" del diagrama)
// ============================================================
namespace Application {

class App {
    HAL::Button       button_;
    Control::Traffic  traffic_;

    // Log circular de eventos (últimas 6 líneas)
    static constexpr int LOG_LINES = 6;
    char log_[LOG_LINES][128];
    int  logHead_ = 0;

    void pushLog(const char* msg){
        snprintf(log_[logHead_], 128, "%s", msg);
        logHead_=(logHead_+1)%LOG_LINES;
    }

    void drawPanel(){
        auto info = traffic_.getDisplay();

        Terminal::clearScreen();
        // ── Título ──────────────────────────────────────────
        printf("\033[1m╔══════════════════════════════════════════════╗\033[0m\n");
        printf("\033[1m║      Semáforo Peatonal — PASO 7 (final)      ║\033[0m\n");
        printf("\033[1m╚══════════════════════════════════════════════╝\033[0m\n\n");

        // ── LEDs visuales ───────────────────────────────────
        auto drawLed = [](const char* ansi, const char* name, bool on){
            if(on)
                printf("  %s  ●  %s  \033[0m  ← encendido\n", ansi, name);
            else
                printf("  \033[2m  ○  %s    \033[0m  (apagado)\n", name);
        };
        drawLed("\033[41;97m","ROJO  ", info.redOn);
        drawLed("\033[43;30m","AMARIL", info.yellowOn);
        drawLed("\033[42;30m","VERDE ", info.greenOn);

        // ── Countdown ───────────────────────────────────────
        printf("\n  Estado: \033[1m%s\033[0m  |  Tiempo restante: \033[1m%u.%u s\033[0m\n",
               info.stateName, info.remainingMs/1000, (info.remainingMs%1000)/100);

        // ── Solicitud ───────────────────────────────────────
        if(info.crossRequested)
            printf("  \033[36m✓ Cruce solicitado — se ejecutará en el próximo ROJO\033[0m\n");
        else
            printf("  \033[2m  (sin solicitud de cruce pendiente)\033[0m\n");

        // ── Log de eventos ──────────────────────────────────
        printf("\n  \033[1mEventos recientes:\033[0m\n");
        for(int i=0;i<LOG_LINES;i++){
            int idx=(logHead_+i)%LOG_LINES;
            if(log_[idx][0])
                printf("  %s\n", log_[idx]);
        }

        // ── Instrucciones ───────────────────────────────────
        printf("\n  \033[2mSPACE = solicitar cruce peatonal   |   Ctrl+C = salir\033[0m\n");
        fflush(stdout);
    }

public:
    App() : button_(0), traffic_(button_) { memset(log_,0,sizeof(log_)); }

    void init(){
        button_.init();
        traffic_.init();
        pushLog(traffic_.popEvent());
    }

    // Super-loop (del diagrama):
    //   1. button.update()
    //   2. traffic.update()  → consulta isRequested()
    void run(){
        init();
        uint32_t lastDraw = 0;
        while(true){
            button_.update();
            traffic_.update();

            if(traffic_.hasEvent()) pushLog(traffic_.popEvent());

            // Redibujar panel cada 200 ms
            uint32_t now=HAL::Timer::millis();
            if(now-lastDraw>=200){ lastDraw=now; drawPanel(); }

            HAL::Timer::delay(20);
        }
    }
};

} // namespace Application

// ============================================================
//  main
// ============================================================
int main(){
    signal(SIGINT,onSignal); signal(SIGTERM,onSignal);
    Terminal::enableRaw();
    Application::App app;
    app.run();
    Terminal::disableRaw();
    return 0;
}
