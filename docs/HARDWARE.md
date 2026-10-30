# Cali Camping Unit – Hardware

Version 1.0 | 30. Oktober 2026 | Grundlage: Stückliste rev b (27.10.2026), Pin-Check Nr. 133, Software-Spezifikation 1.0

Preise sind Budgetwerte in CHF, keine Offerten. Status «Bestätigen/Anfragen/Prüfen» heisst: vor der Bestellung klären.

## 1. Übersicht

```
               12 V Zweitbatterie ── 7,5 A ── Verpolschutz ── TVS ──┬── DCDC-USB 5,1 V (Enable) ── Pi 5 + SM-H-001 + SM-I-009 + H-Relais + Display
                                                                    ├── Pololu 5 V (immer an) ── XIAO ESP32-S3 (Wake) + Touch-Controller
                                                                    └── 2 A ── COM K1–K4
Fahrzeug T32b/T6ap/T2p ── Breakout X1–X4 ── Patchkabel ── SM-H-001 (K1–K4, AI1–AI8) / SM-I-009 (DI1–DI16) / H1–H4
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
| 11 | I/O | Home Automation HAT | Sequent SM-H-001 | 1 | Geplant | 80.00 | 80.00 | K1–K4, AI1–AI8 | Relais-COM gemeinsam je Vierergruppe; K5–K8 frei |
| 13 | I/O | 16 LV-Digitaleingänge | Sequent SM-I-009 | 1 | Geplant | 60.00 | 60.00 | DI1–DI16 (3–24 V) | abgesetzt über Pos. 41 |
| 14 | Heizung | 4-fach-Relaismodul, getrennte COM/NC/NO | Purecrea 420053 | 1 | Geplant | 12.90 | 12.90 | H1–H4 Sollwertleiter | Einschaltpegel bei 3,3-V-Logik prüfen |
| 16 | Heizung | Präzisionswiderstand 1750 Ω 0,1 % | Metallfilm | 1 | Bestellen | 4.00 | 4.00 | Basis der Sollwertleiter |  |
| 17 | Heizung | Präzisionswiderstände 27,4 / 54,9 / 110 / 237 Ω 0,1 % | Metallfilm, je 1 | 1 | Bestellen | 8.00 | 8.00 | Segmente H1–H4 |  |
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
| 41 | I/O | Abgesetzte I²C-Leitung SM-I-009 | LiYCY 4×0,25 mm² + 2×20-Adapter | 1 | Bestätigen | 15.00 | 15.00 | SM-I-009 neben dem Stack | max. ca. 30 cm; I²C-Adressen prüfen |
| 42 | Netz | Patchkabel Cat 6, 0,5 m, flach | – | 1 | Bestellen | 8.00 | 8.00 | Pi ↔ Cerbo GX | neu (ersetzt Pos. 15) |
| 43 | Messung | Messnetzwerk: Pull-ups und Spannungsteiler | Metallfilm 0,1 %, Sortiment | 1 | Bestellen | 12.00 | 12.00 | AI1–AI8 (Kapitel 4) | neu |
| 44 | Strom | ATO-Sicherungshalter + 2 A | Inline | 1 | Bestellen | 6.00 | 6.00 | +12 V an Relais-COM K1–K4 | neu |
| | | **Zwischensumme** | | | | | **1'053.50** | | |
| | | Reserve 15 % | | | | | 158.03 | | |
| | | **Planungsbudget** | | | | | **1'211.52** | | |

Entfallen gegenüber rev b: Pos. 12 USB-RS485 (I²C reicht), Pos. 15 VE.Direct-USB (Daten kommen vom Cerbo).

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

| Kanal | Karte | VW-Pin | Gerät | Schaltet | Bemerkung |
| --- | --- | --- | --- | --- | --- |
| K1 | SM-H-001 Relais 1 | T32b/11 | J768 Dach Befehl A | +12 V über COM (Annahme) | Richtung und Pegel messen (O6) |
| K2 | SM-H-001 Relais 2 | T32b/12 | J768 Dach Befehl B | +12 V | dito |
| K3 | SM-H-001 Relais 3 | T32b/10 | J1029 Kühlanforderung | +12 V | Pegel messen |
| K4 | SM-H-001 Relais 4 | T6ap/3 | J604 S+ | +12 V | S+ bestätigt (T10ba/9) |
| K5–K8 | SM-H-001 Relais 5–8 | – | frei | – | nicht für Sollwertleiter (gemeinsame COM) |
| H1 | Purecrea Relais 1, GPIO17 | T2p | Segment 27,4 Ω | NC überbrückt | Relais an = Segment wirksam |
| H2 | Purecrea Relais 2, GPIO27 | T2p | Segment 54,9 Ω | NC | |
| H3 | Purecrea Relais 3, GPIO22 | T2p | Segment 110 Ω | NC | |
| H4 | Purecrea Relais 4, GPIO23 | T2p | Segment 237 Ω | NC | Summe max. 2179,3 Ω |

**COM K1–K4:** Die vier Relais teilen sich eine COM. Annahme: alle schalten +12 V über die 2-A-Sicherung (Pos. 44). Zeigt die Messung, dass ein Eingang nach Masse geschaltet werden muss, braucht dieser Kanal ein Relais aus der zweiten Gruppe (K5–K8) mit COM auf Masse.

**Sollwertleiter:** T2p/1 → 1750 Ω → Segment H1 → H2 → H3 → H4 → T2p/2. Jedes Segment hat einen NC-Kontakt parallel. Stromlos sind alle Segmente überbrückt: 1750 Ω.

### 4.2 Digitaleingänge (SM-I-009, 3–24 V)

| Kanal | VW-Pin | Signal | Bemerkung |
| --- | --- | --- | --- |
| DI1 | T32b/15 | J768 Rückmeldung A | |
| DI2 | T32b/14 | J768 Rückmeldung B | |
| DI3 | T32b/9 | Kühlbox Diagnose Klemme D | low-aktive Pulse; Pegel messen |
| DI4 | T6ap/2 | J604 Betriebsanzeige | |
| DI5 | T32b/22 | A11 Ladegerät Status | evtl. ohne Funktion (O5) |
| DI6 | T32b/3 | Geschwindigkeitssignal J285 | Frequenz; Eingangsbandbreite des SM-I-009 prüfen |
| DI7 | T32b/27 | Klemme 58d (mit dimmbarer Instrumentenbeleuchtung) | |
| DI8 | T32b/28 | Klemme 86s | |
| DI9 | T32b/29 | J519 T73a/3, Hypothese D+ | messen |
| DI10 | T32b/30 | Zündung 15a (SC34) | zusätzlich entkoppelt an Wake-MCU |
| DI11 | T6ap/4 | 58d Variante ohne dimmbare Beleuchtung | nur falls belegt |
| DI12 | T6ap/6 | 15a Variante (SC32) | Reserve |
| DI13–16 | – | frei | |

### 4.3 Analogeingänge (SM-H-001, 0–10 V)

Die VW-Geber sind Widerstände ohne eigene Speisung. Jeder bekommt ein Messnetzwerk (Pos. 43): Pull-up von der 5,1-V-Schiene, Geber-Ende B auf Masse, AI misst Ende A. AI5 misst die 5,1-V-Schiene selbst, damit die Software ratiometrisch rechnet.

| Kanal | VW-Pin | Geber | Messnetzwerk |
| --- | --- | --- | --- |
| AI1 | T32b/7 (A), /8 (B) | G817 NTC Kühlbox | Pull-up Wert nach Messung des NTC (`TODO_MEASURE`) |
| AI2 | T32b/20, /21 | G120 Frischwasser, 4 Stufen | Pull-up nach Messung der Stufenwiderstände |
| AI3 | T32b/18, /19 | G126 Abwasser, voll/nicht voll | dito |
| AI4 | T32b/16, /17 | G249 NTC aussen | dito |
| AI5 | – | Referenz 5,1-V-Schiene | Teiler 1:1 |
| AI6 | T32b/11 | Dachbefehl A (Messmodus) | Teiler 12 V → ≤ 6 V |
| AI7 | T32b/12 | Dachbefehl B (Messmodus) | Teiler 12 V → ≤ 6 V |
| AI8 | T32b/32 | Batteriespannung, Rückfall ohne Cerbo | Teiler 16 V → ≤ 8 V |

**Achtung Messmodus:** Solange das Original-E153 angeschlossen ist, dürfen keine Pull-ups an AI1–AI4 hängen; sie würden die Messung des E153 verfälschen. Im Messmodus die Pull-ups über die Patchstecker abtrennen und nur hochohmig mithören.

### 4.4 Pi-5-Header

| GPIO | Funktion | Bemerkung |
| --- | --- | --- |
| 2 / 3 | I²C SDA/SCL → SM-H-001, SM-I-009, (Touch) | Adressen der Karten prüfen, Jumper setzen |
| 4 / 5 | UART2 TX/RX ↔ XIAO ESP32-S3 | `dtoverlay=uart2-pi5`; mit Belegung des SM-H-001 abgleichen (O4) |
| 17, 27, 22, 23 | H1–H4 | Platzhalter, mit Belegung des SM-H-001 abgleichen (O4) |
| 26 | `ALIVE` → XIAO (high, solange `camperd` läuft) | Power-Good-Ersatz, zusätzlich zur 3,3-V-Schiene |

## 5. Stromversorgung

### 5.1 Leistungsbudget 5 V

| Verbraucher | typ. A | Spitze A | typ. W | Spitze W |
| --- | --- | --- | --- | --- |
| Raspberry Pi 5 + Kühler | 3,20 | 5,00 | 16,3 | 25,5 |
| Display + HDMI-Treiber | 0,26 | 0,45 | 1,3 | 2,3 |
| PCAP-Touch | 0,08 | 0,15 | 0,4 | 0,8 |
| Wake-MCU | 0,03 | 0,10 | 0,2 | 0,5 |
| SM-H-001, SM-I-009, 4 Zusatzrelais | 0,45 | 1,05 | 2,3 | 5,3 |
| Reserve | 0,35 | 0,70 | 1,8 | 3,5 |
| **Gesamt** | **4,37** | **7,45** | **22,2** | **37,8** |

Mit `cog` statt Chromium dürfte der typische Wert für den Pi tiefer liegen. In M1 messen.

### 5.2 Regeln

- Eigene 12-V-Zuleitung mit 7,5 A (Pos. 28). T32b/32 (über SH21, 5 A, 0,5 mm²) nur als Messpunkt, nicht als Speisung.
- Zündung und alle 12-V-Signale nie direkt an einen MCU- oder Pi-GPIO. Zündung zur Wake-MCU über Eingangsstufe (Teiler + Klemmdiode oder Optokoppler).
- Masse der Unit an T32b/31 bzw. T6ap/5 und an die Zuleitungsmasse, sternförmig.

## 6. Vor dem Einbau messen (Daniel)

1. Widerstand T2p am Original-Panel bei Minimum und Maximum
2. Pegel und Polarität an T32b/10, /11, /12 und T6ap/3 im Betrieb
3. Wahrheitstabelle T32b/14 und /15 beim Öffnen und Schliessen
4. T32b/29 bei Motor aus und an (D+?)
5. Widerstände G120 je Stufe, G126 leer/voll, Kennlinien G817 und G249
6. Typenschild Kühlbox-Kompressor und -Elektronik
7. Typenschild MultiPlus, Batteriemonitor vorhanden?
8. Bauraum hinter dem E153-Ausschnitt (max. 18 cm Breite laut Daniel)
