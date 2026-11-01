# Cali Camping Unit (v2)

Replacement for the camping control panel E153 in a VW T6 California (MY 2018).
**v2:** one ESP32-P4 board (Viewe ESP32-P4-Pi-VIEWE) controls roof, fridge and heater and
shows the 1280 × 480 touch UI in LVGL. The previous Raspberry Pi/XIAO design is on branch
[`v1`](../../tree/v1) (its web demo: https://danielsaeuberli.github.io/California-camping-unit/).

Start with `CLAUDE.md`, `docs/SOFTWARE_SPEC.md`, `docs/HARDWARE.md`. Open work: `docs/TODO.md`.

## Build

Host (macOS/Linux) needs CMake ≥ 3.20, Ninja, a C compiler, Python 3 and SDL2
(`brew install cmake ninja sdl2` / `apt install cmake ninja-build libsdl2-dev`).
Dependencies (LVGL 9.2.2, Unity 2.6.0, cJSON 1.7.18) are fetched at pinned versions.

```
make test       # logic and safety tests on the host (Unity)
make sim        # simulator: device UI 1280 × 480 plus a control panel (ignition, driving, DI, AI)
make sim-shot   # headless screenshots into build/
make build      # firmware, needs ESP-IDF >= 5.4 with ESP32-P4 support (idf.py)
make flash
```

The simulator uses `profiles/sim/sim-overrides.json`: invented values so every function can be
shown, marked «Simulation» in the UI. The real profile `profiles/t6-california-a8l-2018.json`
keeps every unmeasured value as `TODO_MEASURE`; with it nothing switches
(`build/host/camping_sim --profile real`).

## Layout

```
docs/       SOFTWARE_SPEC.md, HARDWARE.md, KUEHLBOX.md, MESSBLATT.md, TODO.md, reference/
profiles/   vehicle profile (JSON + schema), sim overlay
firmware/   ESP-IDF project: main/, components/ (logic shared with the host, ui, device stubs)
host/       host build: tests, LVGL/SDL simulator, hal_sim
tests/      unit/, safety/ (one file per S rule)
ui/         reference/ (web_v1 = current UI template), i18n/de.json, fonts/
tools/      i18n_gen.py, gen_fonts.sh
```
