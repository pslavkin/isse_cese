// ============================================================
//  PASO 1 — Clase mínima
//  Objetivo: la sintaxis C++ más básica.
//  Una sola clase App con un método run() que imprime un
//  mensaje y termina.  Equivale al @startuml con "class App".
// ============================================================

#include <cstdio>

// ----------------------------------------------------------
// App — clase principal (esqueleto vacío)
// ----------------------------------------------------------
class App {
public:
    void run() {
        printf("╔══════════════════════════════════╗\n");
        printf("║   Semáforo Peatonal — PASO 1     ║\n");
        printf("║   Clase mínima: App existe.      ║\n");
        printf("╚══════════════════════════════════╝\n");
    }
};

// ----------------------------------------------------------
// Punto de entrada
// ----------------------------------------------------------
int main() {
    App app;
    app.run();
    return 0;
}
