# XIAO ESP32-S3 firmware (master controller)

Since SPEC 2.0 (decision 31.10.2026) the XIAO is the only master for all outputs:
fridge thermostat, heater, roof, undervoltage protection (AI8) and the safety rules
S1–S14. No Wi-Fi, no Bluetooth, no MQTT: Victron is read by the Pi only.
The Pi 5 is only the operator terminal and talks to the XIAO over UART
(`docs/UART_PROTOCOL.md`).

Planned layout (SPEC 3.2), ESP-IDF via PlatformIO (`framework = espidf`):

```
xiao/
  platformio.ini     env esp32-s3 (target) and native (host tests)
  src/               app_main, tasks
  components/
    io/              SM-H-001, SM-I-009, MCP23017
    signals/ modules/ safety/   plain C, no ESP-IDF dependency, injected clock
    power/ link/ store/
  test/              Unity tests, run on the host with `pio test -e native`
```

Not started yet: the switching logic depends on the measurements listed in
`docs/HARDWARE.md` chapter 6 ("nicht bauen, bevor gemessen"). Milestone M5 starts
with the I/O drivers, the UART link and the Pi power control.
