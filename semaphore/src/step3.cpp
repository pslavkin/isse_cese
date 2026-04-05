// ============================================================
//  PASO 3 — Atributos
//  Objetivo: agregar el estado interno (datos) a cada clase.
//  App imprime los valores iniciales de todos los atributos.
//
//  Notas C++:
//  • protected  → accesible desde subclases (Led → RedLed…)
//  • uint8_t / uint32_t → tipos de ancho fijo (<cstdint>)
//  • static constexpr → constante de clase, sin instancia
// ============================================================

#include <cstdio>
#include <cstdint>

// ----------------------------------------------------------
// Timer
// ----------------------------------------------------------
class Timer {
    // Sin atributos de instancia: todos sus métodos son static
};

// ----------------------------------------------------------
// Led — clase base abstracta
// ----------------------------------------------------------
class Led {
protected:
    uint8_t pin_;    // número de pin GPIO
    bool    state_;  // true = encendido
public:
    Led() : pin_(0), state_(false) {}
};

// ----------------------------------------------------------
// Subclases concretas (sin atributos nuevos por ahora)
// ----------------------------------------------------------
class RedLed    : public Led {};
class YellowLed : public Led {};
class GreenLed  : public Led {};

// ----------------------------------------------------------
// Button
// ----------------------------------------------------------
class Button {
private:
    uint8_t pin_;        // pin GPIO del pulsador
    bool    state_;      // true = presionado ahora mismo
    bool    requested_;  // flag: peatón solicitó cruce
public:
    Button() : pin_(0), state_(false), requested_(false) {}
};

// ----------------------------------------------------------
// Traffic
// ----------------------------------------------------------
class Traffic {
private:
    uint8_t  currentState_;  // estado actual (0=ROJO,1=VERDE…)
    uint32_t stateTime_;     // ms en que se entró al estado

    // LEDs por valor (composición fuerte)
    RedLed    red_;
    YellowLed yellow_;
    GreenLed  green_;

public:
    // Constantes de tiempo (ms) para cada fase
    static constexpr uint32_t GREEN_TIME  = 5000;
    static constexpr uint32_t YELLOW_TIME = 1000;
    static constexpr uint32_t RED_TIME    = 5000;
    static constexpr uint32_t BLINKY_TIME =  300;

    Traffic() : currentState_(0), stateTime_(0) {}
};

// ----------------------------------------------------------
// App
// ----------------------------------------------------------
class App {
private:
    Button  button_;
    Traffic traffic_;
public:
    void run() {
        printf("╔══════════════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 3             ║\n");
        printf("║   Atributos inicializados                ║\n");
        printf("╠══════════════════════════════════════════╣\n");
        printf("║  Traffic::GREEN_TIME  = %5u ms         ║\n", Traffic::GREEN_TIME);
        printf("║  Traffic::YELLOW_TIME = %5u ms         ║\n", Traffic::YELLOW_TIME);
        printf("║  Traffic::RED_TIME    = %5u ms         ║\n", Traffic::RED_TIME);
        printf("║  Traffic::BLINKY_TIME = %5u ms         ║\n", Traffic::BLINKY_TIME);
        printf("╠══════════════════════════════════════════╣\n");
        printf("║  Led::pin_   (protected) heredado por    ║\n");
        printf("║  RedLed / YellowLed / GreenLed           ║\n");
        printf("╚══════════════════════════════════════════╝\n");
    }
};

int main() {
    App app;
    app.run();
    return 0;
}
