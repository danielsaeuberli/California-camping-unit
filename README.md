# Cali Camping Unit

Replacement for the E153 camping control panel in a VW T6 California (MY 2018, A8L/A8R).
A Raspberry Pi 5 drives roof, fridge and parking heater, shows tanks, temperatures and
Victron data on a 1280 × 480 touch display. See `CLAUDE.md` and `docs/SOFTWARE_SPEC.md`.

## Quick start (Mac, no hardware)

```
make venv      # once
make dev       # camperd with the simulator, UI on http://localhost:8080
               # simulator controls (ignition, driving, faults …) on http://localhost:8080/sim
make test      # pytest, incl. safety rules S1–S10 in tests/safety/
make lint      # ruff + mypy
```

`make dev` uses invented calibration values from `profiles/sim/sim-calibration.yaml`
so every page shows data. `make dev-uncal` runs the simulator on the real vehicle
profile, where unmeasured values show as «nicht kalibriert».

## Status

Milestone M1 (software part) – see `docs/M1_STATUS.md`.
