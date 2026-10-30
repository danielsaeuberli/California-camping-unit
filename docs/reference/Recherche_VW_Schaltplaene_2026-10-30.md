# Recherche VW-Strom- und Einbaupläne (Ergebnis 30. Oktober 2026)

Quelle: VW-Stromlaufpläne Nr. 133/2–10 und Einbauseite 805/3. Die Pläne bestätigen Leitungswege sehr gut, nennen aber die meisten Spannungen, Ströme und Kennlinien nicht. Diese Werte müssen am eigenen Fahrzeug (MJ 2018) gemessen werden. Mehrere Werte der bisherigen Planung waren Annahmen und bleiben vorläufig.

## Standheizung J604

| Pin | Bestätigt |
| --- | --- |
| T2p/1 | rt/ws 0,35 mm² → J604 T10ba/7 |
| T2p/2 | br/ws 0,35 mm² → J604 T10ba/10 |
| T6ap/3 | ws/ge 0,35 mm² → J604 T10ba/9 |
| T6ap/2 | ge/gr 0,35 mm² → J604 T10ba/2 |

Nicht veröffentlicht: die zehn Widerstandsstufen an T2p; ob mehr Widerstand wärmer oder kälter bedeutet; Spannung/Strom an T6ap/3; Spannung und Verhalten von T6ap/2 bei Start, Heizen und Nachlauf. Die geplante Leiter 1750–2179 Ω und das S+-Relais nach +12 V sind damit nicht validiert. Endgültige Widerstände erst nach Messung am Original-E153 bestellen.

## Kühlbox J698/J1029

- T32b/10 ge 0,5 mm² → Steuereingang T6bj/3
- T32b/9 ge/rt 0,5 mm² → Diagnoseklemme D, T6bj/4
- G817: T32b/8 gn/br, T32b/7 ws/br
- Beide Varianten J698 und J1029 existieren; das Typenschild entscheidet.

Standard-Secop-BD35F an Klemme D: 1–5 Blinkimpulse, ca. 250 ms je Impuls, ca. 4 s zwischen Gruppen: 1 Batterieschutz, 2 Lüfter-Überstrom, 3 Motorstart, 4 Mindestdrehzahl, 5 Übertemperatur Elektronik. **Code 6 ist beim Standard-BD35F nicht definiert**; «G817 abgezogen = Code 6» wäre eine VW-Auslegung (J698/J1029 oder E153), testen.

Offen: Ein/Aus-Spannung und Strom an T32b/10; elektrische Pegel an Klemme D (vermutlich Low-Side-Ausgang, für VW nicht belegt); Kennlinie G817; Modell von Kompressor und Elektronik.

## Dach J768

| E153 | Leitung | Ziel |
| --- | --- | --- |
| T32b/12 | ws 0,5 mm² | T6bx/1 → J768 T32d/23 |
| T32b/11 | gn 0,5 mm² | T6bx/2 → J768 T32d/22 |
| T32b/15 | vi/br 0,5 mm² | T6bx/5 → J768 T32d/9 |
| T32b/14 | ge/br 0,5 mm² | T6bx/6 → J768 T32d/10 |

VW nennt weder, welcher Befehl öffnet, noch die Wahrheitstabelle der Rückmeldungen. «/12 öffnet, /11 schliesst» ist unbestätigt, ebenso Pegel von 4,5/12 V. J768 bleibt für V118 und G491–G494 zuständig.

## Fahrzeugsignale

| Pin | Ergebnis |
| --- | --- |
| T32b/3 | ws/vi → J285 Geschwindigkeitsausgang über B464. Amplitude und Pulse/km nicht angegeben. |
| T32b/29 | über B478 an J519 T73a/3 und an die Spule des Zweitbatterie-Laderelais J713: Laderelais-/Motor-läuft-Befehl, kein rohes D+. Schaltet vermutlich nach Bordspannung, wenn J713 angesteuert wird; messen. |
| T32b/28 | B518, «86s»: bestätigt. Schlüssel ab = aus, gesteckt = an; messen. |
| T32b/27 | B341, Klemme 58d |
| T6ap/4 | B340, Klemme 58d. Beide Wege gezeigt; die Ausstattung entscheidet, welche Kontakte belegt sind. |
| T32b/30 | 15a über SC34 |
| T6ap/6 | 15a über SC32 |
| T32b/22 | vi 0,5 mm² direkt von A11 T6bp/1; Polarität undokumentiert |

58d führt oft die gedimmte Instrumentenbeleuchtung, eventuell als PWM: Wellenform aufzeichnen.

## Tanks und Temperaturen

G120 ist ein Mehrkontakt-Stufengeber (diskrete Stufen), G126 praktisch binär, G249 und G817 Widerstands-Temperaturfühler. Keine verlässlichen Widerstandswerte gefunden: messen.

## Strom

T32b/32 rt/ge 0,5 mm² über SH21 (5 A). T32b/32 gegen Batteriepol misst Sicherung, Stecker und Leitung zusammen; SH21 allein direkt über beide Sicherungsklemmen messen. MultiPlus, SmartShunt/BMV und Kompressor: Typenschilder fotografieren; die Geräteliste des Cerbo GX zeigt Monitor und MultiPlus-Modell.

## SM-H-001

Eingangsschaltung mit Pull-up auf der Karte, gemeldet 15 kΩ: ein unbenutzter Analogeingang liegt nahe 3,3 V. Notieren: PCB-Revision, Firmware, offene Spannung an jedem vorgesehenen Analogeingang.

## Steckerlage (Einbauseite 805/3, Rückseite E153)

- T32b: Pin 1 oben links, 16 oben rechts, 17 unten links, 32 unten rechts
- T6ap: Pin 1 oben links, 3 oben rechts, 4 unten links, 6 unten rechts
- T2p: senkrecht, Pin 2 über Pin 1

Keine Bestellnummern der Gehäuse: Herstellermarkierungen und Codierrippen fotografieren.

## PR-Codes und Bauraum

A8L/A8R wählen die Schaltplanfamilie «Campingausrüstung»; 9CE/9CI können Licht- und Ladezweige ändern. PR-Codes vom Datenaufkleber notieren, bevor Pins 27–30 festgelegt werden. Keine veröffentlichten Masse des Hohlraums hinter E153: Breite, Höhe, Tiefe, Biegeraum und Halterungen messen.

## Messliste

1. J604: T2p je Anzeigestufe; T6ap/2 und /3 Spannung, Strom, Zeitverlauf
2. Kühlbox: /9 und /10 Spannung, Strom, Zeitverlauf; G817-Kennlinie; beide Typenschilder
3. Dach: /11, /12, /14, /15 gleichzeitig in allen vier Dachzuständen
4. Fahrzeug: /3 Frequenz, /29, /28, beide 58d, beide 15a
5. Geber: G120, G126, G249
6. Typenschilder/Fotos: A11, MultiPlus, Victron-Monitor, SM-H-001, alle drei E153-Stecker
7. Bauraum hinter E153

Planungsstand: Ausgangsstufen für Heizung, Kühlbox und Dach sind «Messung ausstehend», nicht zum Bau freigegeben.
