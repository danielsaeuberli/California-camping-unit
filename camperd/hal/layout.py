"""Channel layout (HARDWARE.md 4).

- SM-H-001 (Home Automation): relays K1-K8, analog inputs AI1-AI8
- SM-I-009 (16 LV digital inputs): DI1-DI16
- Purecrea 4-relay module on Pi GPIOs: heater setpoint ladder H1-H4
"""

from __future__ import annotations

N_RELAY = 8
N_AI = 8
N_DI = 16
N_GPIO = 4
