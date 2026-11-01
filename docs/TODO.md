# TODO (Stand 1. November 2026)

## Software

| Meilenstein | Inhalt | Stand |
| --- | --- | --- |
| M1 | Repo, Profil + Schema, signals/modules/safety/state_bus/control, Host-Tests, Simulator mit Übersicht und Dach | **fertig** (dazu Kühlbox-Seite); `make test` grün |
| M2 | ganze Oberfläche im Simulator nach `ui/reference/web_v1`: Batterie und Strom, Heizung, Wasser, Einstellungen, Expertenansicht; Tag/Nacht; Rückkehr zur Übersicht | offen |
| M3 | Gerät: Board, Display, Touch, Hintergrundbeleuchtung | wartet auf Hardware und Viewe-Antwort (O15, O16) |
| M4 | Treiber SM-I-009, SM-H-001, Sollwertleiter; Cerbo-MQTT | wartet auf Hardware auf dem Tisch |
| M5 | Messmodus, SD-Logger, Kalibrier-Assistent, Webui | offen |
| M6 | (Daniel) Messungen am Fahrzeug → `docs/MESSBLATT.md` | offen |
| M7 | Betriebszustände, Light Sleep, Unterspannung, Ruhestrom | offen |
| M8 | Schaltfunktionen freigeben (Heizung, Kühlbox, Dach) | erst nach M6 |
| M9 | OTA, Ereignislog, Feinschliff | offen |

Kleinere Punkte:
- Einstellungen (Kühlbox an/aus, Soll, Heizstufe) in LittleFS speichern; Verhalten nach Neustart festlegen.
- Uhrzeit: RTC/SNTP (die Kopfzeile zeigt bis dahin die Laufzeit).
- S8 (kein Display-Aus während Dach/Pumpe) mit dem Power-Modul umsetzen und testen; S9, S10 auf dem Gerät.
- Energie-Seite: Ladephasen je Gerät (MPPT, Orion XS, MultiPlus) wie in v1 lesen (SPEC 5 erweitern).
- Firmware-Build in CI, sobald ESP-IDF mit P4 im Container verfügbar ist.

## Hardware-Blocker (Messungen, Unterlagen)

- Kühlbox: Typenschilder Kühlbox/Kompressor/Elektronik, Pegel T32b/9 und /10, G817-Kennlinie, Sollbereich/Hysterese am E153 (`docs/KUEHLBOX.md`, O21).
- Ausgangsart K1–K4 (O22).
- Heizung: zehn Stufen am T2p, Regelrichtung, Zuordnung T6ap/2 und /3 (O23).
- Dach: Wahrheitstabelle T32b/14/15, Richtung K1/K2, Pumpen-Nachlauf (O6).
- Geschwindigkeit T32b/3, D+ T32b/29, 58d, Tankgeber G120/G126, G249.
- Viewe: MIPI-Version, 2 Lanes, PWM, Touch-INT, Init-Sequenz (O15).
- Cerbo: MQTT über den Access Point (O10).
