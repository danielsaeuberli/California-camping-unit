# Prüfstand: Strommessung Sequent-Karten (M5a)

Zweck: entscheiden, ob SM-H-001 und SM-I-009 dauernd an der Standby-Schiene hängen dürfen (Plan A) oder nur bei laufendem Pi bzw. Zündung (Plan B, HARDWARE.md Kapitel 0, O29). Nebenbei klären: Brauchen die Karten 3,3 V vom Pi-Header (O22)?

Dauer: etwa ein Abend. Keine Fahrzeugverbindung.

## Material

- Labornetzteil, einstellbar 5,0 V und 12–13,2 V, Strombegrenzung
- Multimeter mit mA-Bereich, möglichst 0,1 mA Auflösung, in Serie in der Plusleitung
- Pololu D24V22F5 (Pos. 9), falls schon da; sonst Schritte 1–3 direkt mit 5,0 V vom Netzteil
- Pi 5 mit dem bisherigen Image (nur als I²C-Gegenstelle am Tisch) und `tools/bench_check.py`
- Jumperkabel Buchse–Buchse

## Anschluss der Karte ohne Pi-Header

Die Karte steckt **nicht** auf dem Pi. Pinnummern am 40-pol. Header der Karte:

| Header-Pin | Signal | Anschluss |
| --- | --- | --- |
| 2 (oder 4) | 5 V | Netzteil 5,0 V über das Multimeter (mA) |
| 6 | GND | Netzteil Minus |
| 3 / 5 | SDA / SCL | nur in Schritt 2: an Pi-Pin 3 / 5 |
| 9 | GND | nur in Schritt 2: an Pi-Pin 9 (gemeinsame Masse) |
| 1, 17 | 3,3 V | **frei lassen**, nicht mit dem Pi verbinden |

Die Karte erst speisen, dann I²C anschliessen. I²C nur verbinden, wenn Pi und Karte beide eingeschaltet sind.

## Schritte

**1. SM-H-001 allein, 5,0 V, ohne I²C.** Strom nach 30 s ablesen (Relais aus). Leuchten LEDs? Notieren.

**2. SM-H-001 mit I²C vom Pi** (SDA, SCL, GND wie oben, ohne 3,3 V):
- `i2cdetect -y 1`: Erscheint `0x28`? Ja → die Karte läuft ohne 3,3 V vom Header (O22 erledigt). Nein → O22 offen, melden.
- `python3 tools/bench_check.py`: Revision, 3,3-V-Schiene, offene AI-Spannungen notieren.
- `python3 tools/bench_check.py --relay-test` (nur am Tisch): Strom mit einem angezogenen Relais ablesen. Differenz zu Schritt 1 = Spulenstrom.

**3. SM-I-009 allein, 5,0 V.** Ruhestrom nach 30 s. Dann mit I²C wie in Schritt 2: `i2cdetect -y 1` zeigt `0x27`? Einen Eingang mit 12 V belegen: ändert sich der 5-V-Strom? Den Strom auf der 12-V-Seite des Eingangs ebenfalls notieren (erwartet rund 5 mA, HARDWARE.md 4.2).

**4. Wandler.** D24V22F5 an 12,0 V ohne Last: Strom auf der 12-V-Seite. Dann beide Karten am Wandler-Ausgang (Relais aus, kein Eingang belegt), 12-V-Strom bei 12,0 V und 13,2 V.

**5. Später mit Firmware (M5):** XIAO ohne Funk dazu, gleiche Messung. Erst dann ist der Wert «geparkt mit Regelung» vollständig.

## Protokoll

| Nr. | Messung | Spannung | Strom | Bemerkung |
| --- | --- | --- | --- | --- |
| 1 | SM-H-001 Ruhe | 5,0 V | | LEDs? |
| 2a | SM-H-001 antwortet auf 0x28 ohne 3,3 V vom Header | – | ja / nein | O22 |
| 2b | SM-H-001 Revision, Schiene, offene AI | – | | `bench_check.py` |
| 2c | SM-H-001 ein Relais an | 5,0 V | | Spulenstrom = 2c − 1 |
| 3a | SM-I-009 Ruhe | 5,0 V | | |
| 3b | SM-I-009 antwortet auf 0x27 ohne 3,3 V vom Header | – | ja / nein | O22 |
| 3c | SM-I-009 ein Eingang an 12 V: 5-V-Seite / 12-V-Seite | 5,0 / 12 V | | |
| 4a | D24V22F5 ohne Last | 12,0 V | | |
| 4b | D24V22F5 + beide Karten | 12,0 V | | |
| 4c | dito | 13,2 V | | |

## Entscheid

Budget geparkt 0,5–1 W an 12 V: XIAO ohne Funk etwa 0,2 W, Wandler und Touch etwa 0,1 W. Für die beiden Karten bleiben damit rund 0,3–0,6 W an 12 V, also etwa **60 mA an 5 V** zusammen (Messung 1 + 3a).

- **≤ 60 mA:** Plan A, die Karten bleiben an der Standby-Schiene.
- **> 60 mA:** Plan B, die Karten werden geschaltet; die XIAO bekommt eigene Ein- und Ausgänge für Thermostat und Heizung (HARDWARE.md Kapitel 0). Vor dem Umbau melden.

Ergebnis in SPEC O22 und O29 eintragen.
