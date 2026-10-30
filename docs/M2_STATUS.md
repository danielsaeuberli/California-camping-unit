# M2 status

Stand: 30. Oktober 2026 · Hardware wie geplant: **SM-H-001** (K1–K8, AI1–AI8) + **SM-I-009** (DI1–DI16) + **Purecrea-Relaismodul** (H1–H4 an GPIO)

Eine Zwischenversion mit zwei SM-H-001 ist verworfen: Deren Opto-Eingänge belasten die Fahrzeugsignale mit 10–13 mA statt rund 5 mA (SM-I-009), und die Sollwertleiter braucht Öffner mit getrennter COM (Purecrea), die das SM-H-001 nicht hat.

## Erledigt (ohne Hardware geprüft)

| Teil | Ort | Prüfung |
| --- | --- | --- |
| Treiber SM-H-001: Relais über SET/CLR mit Rücklesen, ADC (mV), 3,3-V-Schiene, Einschaltzustand der Relais | `camperd/hal/sm_h001.py` | `tests/test_hw_hal.py` gegen ein Register-Modell |
| Treiber SM-I-009: Eingänge über das Port-Register (V2 und V3), Flankenzähler auf V3 | `camperd/hal/sm_i009.py` | dito |
| Hardware-HAL: I²C im Worker-Thread, ein Bus-Lock, All-off auch bei hängendem Bus (S10), Leiter H1–H4 und ALIVE über libgpiod v2; Leiter ohne geprüfte Polarität nie angesteuert | `camperd/hal/hw.py` | dito |
| Cerbo-GX-Client (MQTT, nur lesen, Keepalive auf `R/…`, nie `W/…`) | `camperd/hal/cerbo_mqtt.py` | `tests/test_cerbo.py` |
| **Freigabe je Funktion** (`release.*` im Profil, alle `false`): Dach, Kühlbox, Heizung schalten nie, solange die Messung aussteht | `camperd/core.py` | `tests/safety/test_release_gate.py` |
| Widerstandsgeber ratiometrisch gegen die 3,3-V-Schiene der Karte, Korrektur Eingangsimpedanz, Spannung über der Schiene → unplausibel; Warnung bei Teilerkanal > 3,0 V | `camperd/modules/sensors.py`, `camperd/core.py` | `tests/test_signals.py`, `tests/test_modules.py` |
| Geschwindigkeit über den Hardware-Zähler des SM-I-009 V3 (O14) | `camperd/modules/sensors.py` | `tests/test_modules.py` |
| 58d als möglicher PWM: «hoch im Fenster von 500 ms» = an | `camperd/modules/sensors.py` | `tests/test_modules.py` |
| Dach nur mit Zündung (wie Original-E153) | `camperd/modules/roof.py` | S4-Tests |
| Sollwertleiter in Serie, Werte `TODO_MEASURE`, bleibt stromlos bis gemessen | `camperd/modules/heater.py` | S1-, S7-Tests |
| Prüfstand-Skript (SM-H-001) | `tools/bench_check.py` | – |
| Doku: HARDWARE.md 1.2, SPEC 1.1, Profil, Recherche in `docs/reference/` | – | – |

88 Tests grün, ruff und mypy sauber. Register und Zugriffsmuster stammen aus dem Herstellercode (`SequentMicrosystems/ioplus-rpi`, `16inpind-rpi`); die Adressen sind durch Kompilieren der Header geprüft.

## Am Prüfstand (Daniel, Reihenfolge)

1. SM-H-001 Stack 0 (0x28), SM-I-009 Stack 0 (0x27). `i2cdetect -y 1` zeigt beide.
2. `python3 tools/bench_check.py`: Revision, 3,3-V-Schiene, Einschaltzustand der Relais, offene AI-Eingänge (≈ 3,3 V = 15-kΩ-Pull-up bestückt, O16).
3. `python3 tools/bench_check.py --relay-test` **nur am Tisch**.
4. Purecrea-Modul: welcher Eingangspegel zieht an (3,3-V-Logik)? Ins Profil `outputs.heater_ladder.gpio_active` und die passende Zeile in `deploy/config.txt` aktivieren.
5. Prüfbrett an DI und AI: `camperd --hal hw`, Expertenansicht. Teilerkanäle bis 16 V, Anzeige ≤ 3,0 V.
6. Cerbo per Ethernet (192.168.77.1, MQTT im LAN ein): Victron-Werte in der Oberfläche.

## Offen

- **Messung am Fahrzeug** (HARDWARE.md 6, `docs/reference/Recherche_VW_Schaltplaene_2026-10-30.md`): Heizung, Kühlbox und Dach bleiben bis dahin nicht freigegeben.
- **O12:** Last der SM-I-009-Eingänge (ca. 5 mA) auf J768-Rückmeldungen, Klemme D, Betriebsanzeige vorher prüfen.
- **O15–O18** (SPEC Kapitel 12).
- Fridge-Code 6: kein BD35F-Standard; Anzeige als «Bedeutung unbestätigt».
- Messmodus (M3): parallel zum Original-E153 nicht über die Karten mithören; Multimeter oder Logikanalysator am Breakout.
