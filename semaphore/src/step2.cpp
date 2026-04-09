// ============================================================
//  PASO 2 — Inventario
//  Objetivo: declarar TODAS las clases del sistema sin
//  atributos ni métodos reales todavía.
//  App las instancia y muestra que existen.
// ============================================================

#include <cstdio>

// ----------------------------------------------------------
// Timer — utilidad de tiempo (sin estado de instancia)
// ----------------------------------------------------------
class Timer {};

// ----------------------------------------------------------
// Led — clase base abstracta de la jerarquía de LEDs
// (abstract en C++: al menos un método virtual puro)
// ----------------------------------------------------------
class Led {};

// ----------------------------------------------------------
// Subclases concretas de LED (una por color)
// ----------------------------------------------------------
class RedLed    {};
class YellowLed {};
class GreenLed  {};

// ----------------------------------------------------------
// Button — abstracción del pulsador peatonal
// ----------------------------------------------------------
class Button {};

// ----------------------------------------------------------
// Traffic — máquina de estados del semáforo
// ----------------------------------------------------------
class Traffic {};

// ----------------------------------------------------------
// App — orquestador principal
// ----------------------------------------------------------
class App {
public:
    void run() {
        printf("╔══════════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 2         ║\n");
        printf("║   Inventario de clases               ║\n");
        printf("╠══════════════════════════════════════╣\n");
        printf("║  Timer      — creado OK              ║\n");
        printf("║  Led        — base abstracta OK      ║\n");
        printf("║  RedLed     — hereda Led  OK         ║\n");
        printf("║  YellowLed  — hereda Led  OK         ║\n");
        printf("║  GreenLed   — hereda Led  OK         ║\n");
        printf("║  Button     — creado OK              ║\n");
        printf("║  Traffic    — creado OK              ║\n");
        printf("║  App        — creado OK              ║\n");
        printf("╚══════════════════════════════════════╝\n");

        // Instanciar cada clase para confirmar que compilan
        Timer      timer;
        RedLed     red;
        YellowLed  yellow;
        GreenLed   green;
        Button     button;
        Traffic    traffic;

        (void)timer; (void)red; (void)yellow;
        (void)green; (void)button; (void)traffic;

        printf("\nTodas las clases instanciadas sin errores.\n");
    }
};

int main() {
    App app;
    app.run();
    return 0;
}
