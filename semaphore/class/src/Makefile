# ============================================================
#  Makefile — Semáforo Peatonal (pasos evolutivos)
#
#  Uso:
#    make STEP=1        → compila y ejecuta step1.cpp
#    make STEP=7        → compila y ejecuta step7.cpp
#    make build STEP=4  → solo compila, sin ejecutar
#    make clean         → borra todos los binarios
#    make all           → compila los 7 pasos (no ejecuta)
#
#  Pasos disponibles: 1 2 3 4 5 6 7
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
BINDIR   := bin

# Paso por defecto si no se pasa STEP
STEP ?= 7

SRC  := step$(STEP).cpp
BIN  := $(BINDIR)/semaphore_step$(STEP)

# ── Targets principales ─────────────────────────────────────

.PHONY: all build run clean help

## Compilar Y ejecutar el paso indicado (target por defecto)
run: build
	@echo ""
	@echo "═══════════════════════════════════════"
	@echo "  Ejecutando PASO $(STEP) ..."
	@echo "  (Ctrl+C para salir)"
	@echo "═══════════════════════════════════════"
	@echo ""
	@./$(BIN)

## Solo compilar (sin ejecutar)
build: $(BINDIR) $(BIN)

$(BIN): $(SRC)
	@echo "  [CC] $(SRC) → $(BIN)"
	@$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)
	@echo "  [OK] Compilado: $(BIN)"

$(BINDIR):
	@mkdir -p $(BINDIR)

## Compilar todos los pasos de una vez
all: $(BINDIR)
	@for s in 1 2 3 4 5 6 7; do \
	    echo "  [CC] step$$s.cpp → $(BINDIR)/semaphore_step$$s"; \
	    $(CXX) $(CXXFLAGS) -o $(BINDIR)/semaphore_step$$s step$$s.cpp || exit 1; \
	done
	@echo ""
	@echo "  [OK] Todos los pasos compilados en $(BINDIR)/"

## Borrar binarios
clean:
	@rm -rf $(BINDIR)
	@echo "  [clean] $(BINDIR)/ eliminado"

## Ayuda
help:
	@echo ""
	@echo "  Semáforo Peatonal — Pasos evolutivos"
	@echo ""
	@echo "  make [STEP=N]          compilar y ejecutar el paso N  (default: 7)"
	@echo "  make build  [STEP=N]   solo compilar"
	@echo "  make all               compilar los 7 pasos"
	@echo "  make clean             borrar binarios"
	@echo ""
	@echo "  Pasos disponibles:"
	@echo "    1 — Clase mínima (App stub)"
	@echo "    2 — Inventario de clases"
	@echo "    3 — Atributos"
	@echo "    4 — Métodos + LEDs ANSI + Button teclado"
	@echo "    5 — Relaciones (Traffic ↔ Button)"
	@echo "    6 — Decisiones de diseño documentadas"
	@echo "    7 — Diagrama final (namespaces, panel visual)"
	@echo ""
