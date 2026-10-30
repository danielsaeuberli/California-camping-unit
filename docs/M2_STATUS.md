# M2 status

Stand: 30. Oktober 2026 · Hardware rev c: **zwei Sequent SM-H-001** (Karte A Stack 0, Karte B Stack 1)

## Erledigt (ohne Hardware geprüft)

| Teil | Ort | Prüfung |
| --- | --- | --- |
| Treiber SM-H-001: Relais über SET/CLR mit Rücklesen, Opto-Eingänge, ADC (mV), 3,3-V-Schiene, Flankenzähler, Einschaltzustand der Relais | `camperd/hal/sm_h001.py` | `tests/test_hw_hal.py` gegen ein Register-Modell der Karte |
| Hardware-HAL für zwei Karten, I²C im Worker-Thread, ein Bus-Lock, All-off auch bei hängendem Bus (S10), ALIVE-GPIO 26 über libgpiod v2 | `camperd/hal/hw.py`, `camperd/hal/layout.py` | dito |
| Cerbo-GX-Client (MQTT, nur lesen, Keepalive auf `R/…`, nie `W/…`) | `camperd/hal/cerbo_mqtt.py` | `tests/test_cerbo.py` |
| Widerstandsgeber ratiometrisch gegen die gemessene 3,3-V-Schiene der Karte, Korrektur Eingangsimpedanz, Spannung über der Schiene → unplausibel | `camperd/modules/sensors.py` | `tests/test_signals.py`, `tests/test_modules.py` |
| Warnung, wenn ein 12-V-Teilerkanal über 3,0 V liegt | `camperd/core.py` | `tests/test_modules.py` |
| Geschwindigkeit über den Hardware-Zähler der Karte (O14) | `camperd/modules/sensors.py` | `tests/test_modules.py` |
| Sollwertleiter auf K9–K12 als Parallelleiter; bleibt stromlos, solange nicht gemessen | `camperd/modules/heater.py` | S1-, S7-Tests |
| Dach nur mit Zündung (wie Original-E153) | `camperd/modules/roof.py` | S4-Tests |
| Prüfstand-Skript | `tools/bench_check.py` | – |
| Doku: HARDWARE.md 1.1 (inkl. Korrektur 4.3 und Pos. 43), SPEC 1.1, Profil | `docs/`, `profiles/` | – |

81 Tests grün, ruff und mypy sauber.

Register und Zugriffsmuster stammen aus dem Herstellercode (`SequentMicrosystems/ioplus-rpi`); die Adressen sind durch Kompilieren von `ioplus.h` geprüft.

## Am Prüfstand (Daniel, Reihenfolge)

1. Karten stapeln, DIP-Schalter: Karte A Stack 0, Karte B Stack 1. `i2cdetect -y 1` zeigt 0x28 und 0x29.
2. `python3 tools/bench_check.py`: Revisionen, 3,3-V-Schiene, Einschaltzustand der Relais, offene AI-Eingänge (≈ 3,3 V = 15-kΩ-Pull-up bestückt → O16).
3. `python3 tools/bench_check.py --relay-test` **nur am Tisch**: jedes Relais einmal, mit Rücklesen.
4. Prüfbrett an DI und AI: `camperd --hal hw`, Expertenansicht → Signale. Teilerkanäle mit Labornetzteil bis 16 V, Anzeige ≤ 3,0 V.
5. Cerbo per Ethernet, feste IP 192.168.77.1, MQTT im LAN ein: Victron-Werte in der Oberfläche.
6. Gemessene Werte ins Profil: `buses.sm_h001_*.ai_input_ohm`, `pullup_ohm` je Geber, Teiler.

## Offen und vor dem Fahrzeug zu klären

- **O12 (verschärft):** Die Opto-Eingänge laden die Fahrzeugsignale mit etwa 10–13 mA (SM-I-009: 5 mA). J768-Rückmeldungen, Klemme D und Betriebsanzeige Heizung vorher unter 1 kΩ Last messen (HARDWARE.md 4.2).
- **O15:** Stufenzahl und Widerstände des Sollwertgebers am T6 messen; Richtung klären. Bis dahin ist die Heizstufe nicht einstellbar (Heizung ein/aus geht).
- **O16:** 15-kΩ-Pull-up bestückt? Eingangsimpedanz je Revision.
- **O17:** Mindestlast der Relaiskontakte bei Signalstrom.
- **Messmodus (M3):** Parallel zum Original-E153 nicht über die Karte mithören (Pull-ups und 10-mA-Optos belasten die Leitungen); Multimeter oder Logikanalysator am Breakout.
- I²C-Takt: Standard 100 kHz. Erst nach Prüfstand ggf. 400 kHz (`dtparam=i2c_arm_baudrate`), falls die Karten das mitmachen.
- Mehrbyte-Lesezugriffe (Zähler, Schiene) folgen dem Herstellercode; am Prüfstand bestätigen.

## Abweichungen und Entscheide in diesem Schritt

1. **Sollwertleiter als Parallelleiter** (statt Serienleiter mit Öffnern): SM-H-001 hat nur Schliesser mit gemeinsamer COM. Stromlos liegt R0 an.
2. **Referenz der Widerstandsgeber** ist die gemessene 3,3-V-Schiene der Karte (Register 0x3F) statt AI5. AI5 ist frei.
3. **Dachfreigabe nur mit Zündung** (`roof.require_ignition: true`), wie beim Original laut Recherche-Nachtrag.
4. **Heizstufen ≤ 16** statt fest 16; Profil bleibt bei 16 mit `OPEN`-Hinweis auf 10 Stufen (SSP 329).
5. Beim Start setzt `camperd` den Einschaltzustand der Relais beider Karten auf 0, falls er anders war (S1 auch vor dem Booten des Pi).
6. Jeder Relaisbefehl wird zurückgelesen; ein nicht schaltendes Relais ist ein Fehler.
