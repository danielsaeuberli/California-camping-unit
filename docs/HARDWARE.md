# Cali Camping Unit – Hardware

Version 1.1 | 30. Oktober 2026 | Grundlage: Stückliste rev c (2× SM-H-001), Pin-Check Nr. 133, Software-Spezifikation 1.0

Änderungen gegenüber 1.0: zwei SM-H-001 statt SM-H-001 + SM-I-009 + Purecrea-Relaismodul; Sollwertleiter als Parallelleiter auf Karte B; Analogeingänge korrigiert (0–3,3 V, nicht 0–10 V), Messnetzwerk Pos. 43 neu.

Preise sind Budgetwerte in CHF, keine Offerten. Status «Bestätigen/Anfragen/Prüfen» heisst: vor der Bestellung klären.

## 1. Übersicht

```
               12 V Zweitbatterie ── 7,5 A ── Verpolschutz ── TVS ──┬── DCDC-USB 5,1 V (Enable) ── Pi 5 + 2× SM-H-001 + Display
                                                                    ├── Pololu 5 V (immer an) ── XIAO ESP32-S3 (Wake) + Touch-Controller
                                                                    └── 2 A ── COM K1–K4 (Karte A)
Fahrzeug T32b/T6ap/T2p ── Breakout X1–X4 ── Patchkabel ── Karte A (K1–K8, DI1–DI8, AI1–AI8) / Karte B (K9–K16, DI9–DI16, AI9–AI16)
Karte A = SM-H-001 Stack 0 (I²C 0x28), Karte B = SM-H-001 Stack 1 (0x29), beide direkt auf dem Pi gestapelt
Cerbo GX ── Ethernet ── Pi 5
Viewe-Display ── HDMI ── Pi 5;  Touch ── I²C/USB ── Pi 5, IRQ ── XIAO
```

## 2. Bereits im Fahrzeug (nicht kaufen)

| Gerät | Rolle | Zu prüfen |
| --- | --- | --- |
| Victron Cerbo GX | Datenquelle Batterie, Laden, 230 V über MQTT | MQTT im LAN einschalten, feste IP auf Ethernet |
| Victron MultiPlus Compact (Annahme 12/1600/70) | Wechselrichter, Ladegerät, Netzumschaltung | Typenschild |
| Batteriemonitor (SmartShunt/BMV?) | SoC | vorhanden? Ohne Shunt kein SoC (Anzeige «–») |
| J768, J698/J1029, J604, G817, G249, G120, G126 | VW-Originalgeräte, bleiben | – |
| A11 VW-Ladegerät | evtl. durch MultiPlus ersetzt | eingebaut oder nicht (O5) |
| N162 VW-Shunt | wird nicht mehr gebraucht | T32b/25, /26 nur auf Breakout |

## 3. Stückliste

| Pos | Gruppe | Bauteil | Modell | Menge | Status | CHF/Stk | CHF | Funktion | Hinweis |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Rechner | Raspberry Pi 5 4 GB | Raspberry Pi | 1 | Bestellen | 75.00 | 75.00 | Hauptrechner, Oberfläche | 5 V / 5 A |
| 2 | Rechner | Active Cooler | Raspberry Pi | 1 | Bestellen | 7.00 | 7.00 | Kühlung Pi |  |
| 3 | Rechner | microSD 64 GB High Endurance | A2 | 1 | Bestellen | 18.00 | 18.00 | System, /data |  |
| 4 | Rechner | RTC-Batterie | Raspberry Pi | 1 | Bestellen | 6.00 | 6.00 | Uhrzeit ohne Netz |  |
| 5 | Anzeige | 6,86" 480×1280 IPS mit HDMI-Treiberplatine | Viewe UEDX48128068-HMD-H | 1 | Touchpaket prüfen | 40.00 | 40.00 | Anzeige, quer 1280×480 | Panel und Touch passend zusammen bestellen |
| 6 | Anzeige | PCAP-Touch mit I²C + IRQ | Viewe, Anfrage | 1 | Anfragen | 45.00 | 45.00 | Bedienung und Touch-Wake | Controller, IRQ, Ruhestrom schriftlich bestätigen lassen (O9) |
| 7 | Anzeige | Micro-HDMI auf HDMI, 0,2 m, gewinkelt | verriegelbar | 1 | Bestellen | 15.00 | 15.00 | Video |  |
| 8 | Strom | DC/DC 6–34 V → 5,1 V mit Enable | Mini-Box DCDC-USB | 1 | Leistung prüfen | 105.00 | 105.00 | Hauptversorgung 5 V | ≥ 8 A Spitze nötig (Leistungsbudget), thermisch nachweisen |
| 9 | Strom | 5-V-Standby-Wandler | Pololu D36V6F5 | 1 | Bestellen | 25.00 | 25.00 | Dauerversorgung Wake-MCU, Touch |  |
| 10 | Wake | Always-on-MCU | Seeed XIAO ESP32-S3 | 1 | Bestellen | 12.00 | 12.00 | Zündung, Touch-IRQ, DCDC-Enable | Zündung nur über 12-V-Eingangsstufe |
| 11 | I/O | Home Automation HAT | Sequent SM-H-001 | 2 | Geplant | 80.00 | 160.00 | Karte A: K1–K8, DI1–DI8, AI1–AI8; Karte B: K9–K16, DI9–DI16, AI9–AI16 | Stack-Level 0 und 1 per DIP; Board-Revision notieren (Pull-up 15 kΩ?) |
| 16 | Heizung | Präzisionswiderstände Parallelleiter 0,1 % | Metallfilm, 1 Basis + 4 Zweige | 1 | Nach Messung | 12.00 | 12.00 | Sollwert J604 über K9–K12 | Werte aus Messung T2p (Kapitel 6) berechnen |
| 18 | Fahrzeug | Gegenstecker T32b | VW, Codierung am E153 ablesen | 1 | Bestätigen | 35.00 | 35.00 | Originalkabelbaum | Teilenummer nach Gehäusefoto |
| 19 | Fahrzeug | MQS-Kontakte/Reparaturleitungen | 32 + 6 Reserve | 38 | Bestellen | 2.20 | 83.60 | alle 32 Pins |  |
| 20 | Fahrzeug | Gegenstecker T6ap komplett | VW | 1 | Bestätigen | 18.00 | 18.00 | Heizung, Zündung |  |
| 21 | Fahrzeug | Gegenstecker T2p komplett | VW | 1 | Bestätigen | 10.00 | 10.00 | Heizungssollwert |  |
| 22 | Breakout | 8-pol. Panel-Durchführung | Phoenix DFK-MSTB 2,5/8 | 4 | Bestellen | 22.00 | 88.00 | X1–X4 für T32b/01–32 |  |
| 23 | Breakout | 8-pol. Gegenstecker | Phoenix MSTB 2,5/8-STF-5,08 | 8 | Bestellen | 10.00 | 80.00 | 4 innen, 4 aussen |  |
| 24 | Verdrahtung | FLRY-B 0,35 mm², farbig | Meter | 25 | Bestellen | 1.20 | 30.00 | Signalleitungen |  |
| 25 | Verdrahtung | FLRY-B 1,0 mm² rot/braun | Meter | 5 | Bestellen | 2.00 | 10.00 | Versorgung, Masse |  |
| 26 | Verdrahtung | Adermarker 01–32 + Schrumpfschlauch | Set | 1 | Bestellen | 22.00 | 22.00 | Beschriftung beidseitig |  |
| 27 | Verdrahtung | Micro-Fit-Serviceverbinder 12-pol. | Molex | 3 | Bestellen | 12.00 | 36.00 | Module trennbar |  |
| 28 | Strom | ATO-Sicherungshalter + 7,5 A | Inline | 1 | Bestellen | 12.00 | 12.00 | 12-V-Zuleitung Unit | eigene Zuleitung, nicht über SH21 |
| 29 | Strom | Lastabwurf-TVS | Littelfuse SLD15-018 | 1 | Abstimmen | 22.00 | 22.00 | 12-V-Eingang | Klemmspannung mit Wandler abstimmen |
| 30 | Strom | Verpolschutz 10 A | Ideal-Diode-Modul | 1 | Bestätigen | 35.00 | 35.00 | 12-V-Eingang |  |
| 36 | Service | Panel-USB-C-Verlängerung | mit Kappe | 1 | Bestellen | 18.00 | 18.00 | Service |  |
| 42 | Netz | Patchkabel Cat 6, 0,5 m, flach | – | 1 | Bestellen | 8.00 | 8.00 | Pi ↔ Cerbo GX | neu (ersetzt Pos. 15) |
| 43 | Messung | Messnetzwerk: Pull-ups 0805, Teiler 47 k / 10 k, Serienwiderstände, Klemmdioden BAT54S | Metallfilm 0,1 % + BAT54S | 1 | Bestellen | 18.00 | 18.00 | AI1–AI8 (Kapitel 4.3) | Karte hat keinen Eingangsschutz |
| 44 | Strom | ATO-Sicherungshalter + 2 A | Inline | 1 | Bestellen | 6.00 | 6.00 | +12 V an Relais-COM K1–K4 | neu |
| | | **Zwischensumme** | | | | | **1'051.60** | | |
| | | Reserve 15 % | | | | | 157.74 | | |
| | | **Planungsbudget** | | | | | **1'209.34** | | |

Entfallen gegenüber rev b: Pos. 12 USB-RS485 (I²C reicht), Pos. 15 VE.Direct-USB (Daten kommen vom Cerbo). Rev c: Pos. 13 SM-I-009, Pos. 14 Purecrea-Relaismodul, Pos. 17 Seriensegmente und Pos. 41 abgesetzte I²C-Leitung entfallen; Pos. 11 doppelt; Pos. 16 als Parallelleiter.

Noch nicht budgetiert (Menge erst nach Bauraumaufnahme): Pos. 31 Aluträger, 32 Gehäuse, 33 Frontblende für den E153-Ausschnitt, 34 Abstandshalter, 35 EPDM-Dichtung, 37 Testbuchsen, 38 Kabelschutz, 39 Lötstützpunkte, 40 MQS-Crimpwerkzeug. Richtwert zusammen rund CHF 400.

### Prüfstand (für Entwicklung, nicht im Fahrzeug)

| Teil | Zweck | CHF |
| --- | --- | --- |
| Labornetzteil 0–15 V / 5 A | Unit am Tisch, Unterspannung testen | vorhanden? |
| Prüfbrett: 8 Kippschalter (12 V), 4 Potis 0–2 kΩ, 4 LEDs mit Vorwiderstand | DI, Tankgeber/NTC und K1–K4 nachbilden | 25 |
| USB-Logikanalysator 8 Kanäle (optional) | Pulse und Wahrheitstabelle mitschreiben | 20 |

## 4. Kanalbelegung

Quelle der Wahrheit für die Software ist `profiles/t6-california-a8l-2018.yaml`. Diese Tabelle muss damit übereinstimmen.

### 4.1 Ausgänge

Alle Relais des SM-H-001 sind Schliesser (N.O.). Die COM ist laut bisheriger Planung je Vierergruppe gemeinsam (K1–K4, K5–K8 usw.); an der Karte bestätigen.

| Kanal | Karte | VW-Pin | Gerät | Schaltet | Bemerkung |
| --- | --- | --- | --- | --- | --- |
| K1 | A Relais 1 | T32b/11 | J768 Dach Befehl A | +12 V über COM (Annahme) | Richtung und Pegel messen (O6) |
| K2 | A Relais 2 | T32b/12 | J768 Dach Befehl B | +12 V | dito |
| K3 | A Relais 3 | T32b/10 | J1029 Kühlanforderung | +12 V | Pegel messen |
| K4 | A Relais 4 | T6ap/3 | J604 S+ | +12 V | S+ bestätigt (T10ba/9) |
| K5–K8 | A Relais 5–8 | – | frei | – | gleiche COM wie K1–K4 |
| K9–K12 | B Relais 1–4 | T2p | Sollwertleiter Heizung | Zweig parallel | COM-Gruppe nur für T2p/1, nie mit +12 V verbinden |
| K13–K16 | B Relais 5–8 | – | frei | – | zweite COM-Gruppe, z. B. für Ausgänge nach Masse |

**COM K1–K4:** Annahme: alle schalten +12 V über die 2-A-Sicherung (Pos. 44). Zeigt die Messung, dass ein Eingang nach Masse geschaltet werden muss, kommt dieser Kanal auf Karte B, Gruppe K13–K16, mit COM auf Masse.

**Sollwertleiter (Parallelleiter):** Die Serienleiter mit Öffnern aus Version 1.0 geht mit Schliessern und gemeinsamer COM nicht. Neu: T2p/1 an COM der Gruppe K9–K12. Zwischen T2p/1 und T2p/2 liegt immer der Basiswiderstand R0. Jedes Relais schaltet einen Zweig Ri von T2p/1 nach T2p/2 parallel dazu: R = 1 / (1/R0 + Σ bi/Ri). Mit binär gestuften Leitwerten (1/R1 : 1/R2 : 1/R3 : 1/R4 = 1 : 2 : 4 : 8) ergeben sich bis zu 16 Stufen in gleichen Leitwertschritten.

- Stromlos (S1) liegt R0 an. R0 ist der Widerstand der Stufe, die bei ausgeschalteter Unit gelten soll.
- R0 und Ri folgen aus der Messung T2p am Original (Kapitel 6, Punkt 1). Laut SSP 329 hat das Original 10 Stufen (T5); am T6 zählen. Ist der Widerstand am Original bei höherer Temperatur kleiner, ist R0 die tiefste Stufe, sonst die höchste. Dann R0 und die Software-Abbildung Stufe → Bits entsprechend wählen.
- Die Relais schalten Signalstrom (mA). Mindestlast der Relaiskontakte im Datenblatt prüfen (Kontaktsicherheit bei kleinen Strömen).

### 4.2 Digitaleingänge (Opto-Eingänge SM-H-001)

DI1–DI8 = Karte A Opto 1–8, DI9–DI16 = Karte B Opto 1–8. Eingangsstufe laut Sequent: Optokoppler mit 1 kΩ Serienwiderstand, Sperrspannung max. 5 V, Zähler bis 500 Hz.

| Kanal | Karte | VW-Pin | Signal | Bemerkung |
| --- | --- | --- | --- | --- |
| DI1 | A Opto 1 | T32b/15 | J768 Rückmeldung A | |
| DI2 | A Opto 2 | T32b/14 | J768 Rückmeldung B | |
| DI3 | A Opto 3 | T32b/9 | Kühlbox Diagnose Klemme D | low-aktive Pulse; Pegel messen |
| DI4 | A Opto 4 | T6ap/2 | J604 Betriebsanzeige | |
| DI5 | A Opto 5 | T32b/22 | A11 Ladegerät Status | evtl. ohne Funktion (O5) |
| DI6 | A Opto 6 | T32b/3 | Geschwindigkeitssignal J285 | Hardware-Flankenzähler der Karte (bis 500 Hz), löst O14 |
| DI7 | A Opto 7 | T32b/27 | Klemme 58d (mit dimmbarer Instrumentenbeleuchtung) | |
| DI8 | A Opto 8 | T32b/28 | Klemme 86s | |
| DI9 | B Opto 1 | T32b/29 | J519 T73a/3, Hypothese D+ | messen |
| DI10 | B Opto 2 | T32b/30 | Zündung 15a (SC34) | zusätzlich entkoppelt an Wake-MCU |
| DI11 | B Opto 3 | T6ap/4 | 58d Variante ohne dimmbare Beleuchtung | nur falls belegt |
| DI12 | B Opto 4 | T6ap/6 | 15a Variante (SC32) | Reserve |
| DI13–16 | B Opto 5–8 | – | frei | |

**Belastung der Fahrzeugsignale (O12, neu verschärft):** Mit 1 kΩ zieht jeder Eingang bei 12–14,8 V etwa 10–13 mA (SM-I-009: rund 5 mA). Für 15a, 86s, 58d unkritisch. Kritisch können die Ausgänge von J768 (T32b/14, /15), Klemme D der Kühlbox und die Betriebsanzeige der Heizung sein, falls sie schwach treiben. Vor dem Anschliessen messen: Leerlaufspannung und Spannung mit 1 kΩ Last. Bricht ein Signal ein, Serienwiderstand erhöhen (z. B. 2,2–4,7 kΩ extern) und prüfen, ob der Optokoppler noch sicher schaltet.

**Messmodus:** Parallel zum Original-E153 belasten die Opto-Eingänge die Leitungen mit 10 mA. Für das Mithören am Original deshalb zuerst Multimeter oder Logikanalysator am Breakout verwenden (siehe 4.3).

### 4.3 Analogeingänge (SM-H-001, 0–3,3 V)

**Korrektur gegenüber Version 1.0:** Die Analogeingänge messen 0–3,3 V mit 12 bit, nicht 0–10 V. 0–10 V gilt nur für die vier Analogausgänge (DAC) der Karte.

- Sequent User's Guide v3.0: 0–3,3 V, 12 bit, je Eingang 15 kΩ Pull-up auf der Karte für Thermistoren; mit einem parallelen 0805-Widerstand verkleinerbar.
- Schema: je Kanal 51 Ω Serie, 15 kΩ Pull-up und 220 nF, direkt am STM32. **Keine Klemmdioden, kein Teiler.**
- Händlerangabe V4: max. 3 V, 50 kΩ Eingangsimpedanz. Passt nicht zur v3-Anleitung; der Pull-up kann je Revision anders sein. Auslegung: **max. 3,0 V**.
- Karte prüfen: Spannung an einem offenen AI-Eingang gegen Masse messen. Etwa 3,3 V heisst: Pull-up bestückt. Board-Revision notieren.

**Widerstandsgeber (G817, G249, G120, G126):** Geber zwischen AI und Masse, Pull-up von **3,3 V** (der 15-kΩ-Pull-up der Karte oder ein externer Widerstand an die 3,3-V-Schiene der Karte). **Nie von der 5,1-V-Schiene hochziehen:** Ein offener Geber legte sonst 5 V an einen ungeschützten 3,3-V-Eingang. Die Tankgeber brauchen voraussichtlich einen kleineren Pull-up als 15 kΩ für brauchbare Auflösung; Wert nach Messung der Stufenwiderstände. Die Software rechnet ratiometrisch gegen die gemessene 3,3-V-Schiene der Karte (Register 0x3F), ein eigener Referenzkanal entfällt.

**12-V-Kanäle (AI6, AI7, AI8):** Teiler auf **≤ 3,0 V**, nicht ≤ 6 V / ≤ 8 V. Beispiel 47 kΩ / 10 kΩ: etwa 2,6 V bei 14,8 V und 2,8 V bei 16 V. Zusätzlich Serienwiderstand und Klemmung auf 3,3 V (z. B. BAT54S), weil die Karte keinen Eingangsschutz hat. Ist der 15-kΩ-Pull-up bestückt, verschiebt er den Teiler: entweder in die Rechnung aufnehmen oder auf diesen Kanälen entfernen. Die Software meldet «Analogeingang über 3,0 V», wenn ein Teilerkanal die Grenze überschreitet.

| Kanal | Karte | VW-Pin | Geber / Signal | Messnetzwerk |
| --- | --- | --- | --- | --- |
| AI1 | A | T32b/7 (A), /8 (B) | G817 NTC Kühlbox | Pull-up 3,3 V; Wert `TODO_MEASURE` |
| AI2 | A | T32b/20, /21 | G120 Frischwasser, 4 Stufen | Pull-up 3,3 V, kleiner als 15 kΩ; nach Messung |
| AI3 | A | T32b/18, /19 | G126 Abwasser, voll/nicht voll | dito |
| AI4 | A | T32b/16, /17 | G249 NTC aussen | Pull-up 3,3 V; nach Messung |
| AI5 | A | – | frei (Referenz kommt aus der Karte) | – |
| AI6 | A | T32b/11 | Dachbefehl A (Messmodus) | Teiler 47 k / 10 k + Serie + BAT54S |
| AI7 | A | T32b/12 | Dachbefehl B (Messmodus) | dito |
| AI8 | A | T32b/32 | Batteriespannung, Rückfall ohne Cerbo | dito |
| AI9–AI16 | B | – | frei | – |

**Achtung Messmodus:** Der 15-kΩ-Pull-up sitzt auf der Karte und lässt sich mit den Patchsteckern nicht abtrennen. Parallel zum Original-E153 würden AI1–AI4 die Geberleitungen belasten und die Messung des E153 verfälschen. Für die Aufzeichnung am Original deshalb Multimeter oder Logikanalysator am Breakout verwenden, nicht die Karte.

### 4.4 Pi-5-Header

| GPIO | Funktion | Bemerkung |
| --- | --- | --- |
| 2 / 3 | I²C SDA/SCL → 2× SM-H-001 (0x28, 0x29), (Touch) | SM-H-001 nutzt laut Sequent nur I²C (O4 erledigt) |
| 4 / 5 | UART2 TX/RX ↔ XIAO ESP32-S3 | `dtoverlay=uart2-pi5` |
| 17, 27, 22, 23 | frei | früher H1–H4 (Purecrea), entfällt |
| 26 | `ALIVE` → XIAO (high, solange `camperd` läuft) | Power-Good-Ersatz, zusätzlich zur 3,3-V-Schiene |

## 5. Stromversorgung

### 5.1 Leistungsbudget 5 V

| Verbraucher | typ. A | Spitze A | typ. W | Spitze W |
| --- | --- | --- | --- | --- |
| Raspberry Pi 5 + Kühler | 3,20 | 5,00 | 16,3 | 25,5 |
| Display + HDMI-Treiber | 0,26 | 0,45 | 1,3 | 2,3 |
| PCAP-Touch | 0,08 | 0,15 | 0,4 | 0,8 |
| Wake-MCU | 0,03 | 0,10 | 0,2 | 0,5 |
| 2× SM-H-001 (je 50 mA ohne, 700 mA mit allen Relais) | 0,35 | 1,40 | 1,8 | 7,1 |
| Reserve | 0,35 | 0,70 | 1,8 | 3,5 |
| **Gesamt** | **4,27** | **7,80** | **21,7** | **39,6** |

Mit `cog` statt Chromium dürfte der typische Wert für den Pi tiefer liegen. In M1 messen. Die Spitze von 7,8 A liegt knapp unter den ≥ 8 A des DCDC-USB (Pos. 8); in der Praxis sind nie alle 16 Relais gleichzeitig an (höchstens K1/K2, K3, K4 und K9–K12 = 7).

### 5.2 Regeln

- Eigene 12-V-Zuleitung mit 7,5 A (Pos. 28). T32b/32 (über SH21, 5 A, 0,5 mm²) nur als Messpunkt, nicht als Speisung.
- Zündung und alle 12-V-Signale nie direkt an einen MCU- oder Pi-GPIO. Zündung zur Wake-MCU über Eingangsstufe (Teiler + Klemmdiode oder Optokoppler).
- Masse der Unit an T32b/31 bzw. T6ap/5 und an die Zuleitungsmasse, sternförmig.

## 6. Vor dem Einbau messen (Daniel)

1. Widerstand T2p am Original-Panel **je Stufe** (laut SSP 329 zehn Stufen, +10 bis +30 °C), E153-Seite, T2p abgezogen
2. Pegel und Polarität an T32b/10, /11, /12 und T6ap/3 im Betrieb
3. Wahrheitstabelle T32b/14 und /15 beim Öffnen und Schliessen
4. T32b/29 bei Motor aus und an (D+?)
5. Widerstände G120 je Stufe, G126 leer/voll, Kennlinien G817 und G249
6. Typenschild Kühlbox-Kompressor und -Elektronik
7. Typenschild MultiPlus, Batteriemonitor vorhanden?
8. Bauraum hinter dem E153-Ausschnitt (max. 18 cm Breite laut Daniel)
9. Belastbarkeit der Signale an DI1–DI4 mit 1 kΩ (Kapitel 4.2, O12)
10. Beide SM-H-001: Spannung an offenem AI-Eingang (Pull-up bestückt?) und Board-Revision
