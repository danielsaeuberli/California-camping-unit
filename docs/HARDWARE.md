# Cali Camping Unit – Hardware

Version 1.2 | 30. Oktober 2026 | Grundlage: Stückliste rev b (27.10.2026), Pin-Check Nr. 133, Recherche 30.10.2026, Software-Spezifikation 1.1

Änderungen gegenüber 1.0: Analogeingänge korrigiert (0–3,3 V, nicht 0–10 V; Kapitel 4.3, Pos. 43). Ausgangsstufen für Heizung, Kühlbox und Dach sind **«Messung ausstehend», nicht zum Bau freigegeben** (Kapitel 4.1). Sollwertleiter-Werte offen. Version 1.1 (zwei SM-H-001) ist verworfen: Die Opto-Eingänge des SM-H-001 belasten die Fahrzeugsignale mit 10–13 mA statt rund 5 mA, und die Sollwertleiter braucht Öffner mit getrennter COM.

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
| Victron **Cerbo-S GX**, Venus OS v3.85 (VRM, 31.10.2026) | Datenquelle Batterie, Laden, 230 V über MQTT; Bluetooth für den Ruuvi-Tag | MQTT im LAN einschalten; WLAN-AP für den Pi (5.3, O19). Hängt heute im Router-LAN (192.168.105.x); der Pi kommt per WLAN an den AP des Cerbo |
| Victron **MultiPlus Compact 12/1600/70-16** (bestätigt, VE.Bus-Firmware 558, VRM-Instanz 276) | Wechselrichter, Ladegerät, Netzumschaltung; 16-A-Netzumschaltung → `current_limit_a.max: 16` | Typenschild; über **VE.Bus** (RJ45-Patchkabel) an die VE.Bus-Buchse des Cerbo, **nicht** an Ethernet. Modus und Landstrom-Begrenzung über den Cerbo steuerbar (SPEC 5.1) |
| Ruuvi-Tag (Bluetooth) | Innentemperatur und Luftfeuchte, nur Anzeige (SPEC 5.2) | am Cerbo unter Bluetooth-Sensoren einschalten (je nach Venus-Version I/O oder Integrationen), Namen vergeben und in `interior_temp.custom_name` eintragen. Platz im Wohnraum, nicht an der Heizungsdüse, nicht in der Sonne |
| Batteriemonitor: **SmartShunt, geplant** (Daniel, 31.10.2026) | SoC, Restlaufzeit, 12-V-Verbraucher | Heute keiner: Der SoC kommt vom MultiPlus (Schätzung ohne die 12-V-Verbraucher), keine Restlaufzeit, «12-V-Verbraucher» 0 W. Nach dem Einbau am Cerbo unter Einstellungen → System-Setup → Batteriemonitor den SmartShunt wählen und «Hat DC-System» einschalten. In `camperd` nichts zu ändern: Es liest `system/0/Dc/Battery/*`, die Restlaufzeit erscheint dann von selbst |
| Victron **SmartSolar MPPT 100/20** (VRM-Instanz 279) | Solarladen, `system/0/Dc/Pv/Power` | – |
| Victron **Orion XS 12/12-50A** (VRM-Instanz 277, in VRM als «Alternator») | DC-DC-Laden aus der Starterbatterie während der Fahrt, `system/0/Dc/Alternator/Power` | Wert kommt (O21 erledigt). Ihre Eingangsspannung zeigt die Unit als Starterbatterie |
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
| 14 | Heizung | 4-fach-Relaismodul, getrennte COM/NC/NO | Purecrea 420053 | 1 | Geplant | 12.90 | 12.90 | H1–H4 Sollwertleiter | Einschaltpegel bei 3,3-V-Logik und Polarität prüfen (Kapitel 4.4); Mindestlast der Kontakte |
| 16 | Heizung | Präzisionswiderstand Basis R0 0,1 % | Metallfilm | 1 | Messung ausstehend | 4.00 | 4.00 | Basis der Sollwertleiter | Wert erst nach Messung T2p je Stufe; 1750 Ω unbestätigt |
| 17 | Heizung | Präzisionswiderstände Segmente H1–H4 0,1 % | Metallfilm, je 1 | 1 | Messung ausstehend | 8.00 | 8.00 | Segmente H1–H4 | Werte nach Messung; 27,4 / 54,9 / 110 / 237 Ω unbestätigt |
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
| 42 | Netz | Patchkabel Cat 6, flach, Länge nach Einbau | – | 1 | Bestellen | 8.00 | 8.00 | Pi ↔ LAN-Port Camper-Router | Cerbo über dessen WLAN-AP (Kapitel 5.3) |
| 43 | Messung | Messnetzwerk: Pull-ups 0805, Teiler 47 k / 10 k, Serienwiderstände, Klemmdioden BAT54S | Metallfilm 0,1 % + BAT54S | 1 | Bestellen | 18.00 | 18.00 | AI1–AI8 (Kapitel 4.3) | SM-H-001 hat keinen Eingangsschutz |
| 44 | Strom | ATO-Sicherungshalter + 2 A | Inline | 1 | Bestellen | 6.00 | 6.00 | +12 V an Relais-COM K1–K4 | neu |
| | | **Zwischensumme** | | | | | **1'059.50** | | |
| | | Reserve 15 % | | | | | 158.92 | | |
| | | **Planungsbudget** | | | | | **1'218.42** | | |

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

**Status: Messung ausstehend, nicht zum Bau freigegeben.** Die VW-Schaltpläne bestätigen die Leitungswege (Nr. 133/2–10), aber nicht Pegel, Ströme, Richtungen und Widerstandswerte. Bis zur Messung am Original-E153 bleiben die Ausgangsstufen für Dach, Kühlbox und Heizung vorläufig. Die Software schaltet sie erst, wenn sie im Profil freigegeben sind (`release.roof`, `release.fridge`, `release.heater`, alle `false`).

| Kanal | Karte | VW-Pin | Gerät | Schaltet | Bemerkung |
| --- | --- | --- | --- | --- | --- |
| K1 | SM-H-001 Relais 1 | T32b/11 (gn 0,5 → T6bx/2 → J768 T32d/22) | J768 Dach Befehl A | +12 V über COM (Annahme) | Richtung unbestätigt (O6) |
| K2 | SM-H-001 Relais 2 | T32b/12 (ws 0,5 → T6bx/1 → J768 T32d/23) | J768 Dach Befehl B | +12 V (Annahme) | dito; «/12 öffnet, /11 schliesst» ist unbestätigt |
| K3 | SM-H-001 Relais 3 | T32b/10 (ge 0,5 → T6bj/3) | J698/J1029 Kühlanforderung | +12 V (Annahme) | Pegel und Strom messen; Typenschild entscheidet J698 oder J1029 |
| K4 | SM-H-001 Relais 4 | T6ap/3 (ws/ge 0,35 → J604 T10ba/9) | J604 S+ | +12 V (Annahme) | Funktion S+ bestätigt; Spannung und Strom messen |
| K5–K8 | SM-H-001 Relais 5–8 | – | frei | – | nicht für Sollwertleiter (gemeinsame COM) |
| H1–H4 | Purecrea Relais 1–4, GPIO17/27/22/23 | T2p/1 (rt/ws → T10ba/7), T2p/2 (br/ws → T10ba/10) | Sollwertleiter Segmente | NC überbrückt | Relais an = Segment wirksam; Werte offen |

**COM K1–K4:** Die vier Relais teilen sich eine COM. Annahme: alle schalten +12 V über die 2-A-Sicherung (Pos. 44). Zeigt die Messung, dass ein Eingang nach Masse geschaltet werden muss, braucht dieser Kanal ein Relais aus der zweiten Gruppe (K5–K8) mit COM auf Masse. E153 überwacht seine Ausgänge auf Kurzschluss und Unterbrechung (Fehlercodes 1100–3101); ob J768/J604/J1029 einen Relaiskontakt ohne Fehlermeldung akzeptieren, zeigt erst der Test.

**Sollwertleiter:** T2p/1 → R0 → Segment H1 → H2 → H3 → H4 → T2p/2. Jedes Segment hat einen NC-Kontakt parallel. Stromlos sind alle Segmente überbrückt: R0. Das Original hat **10 Stufen** (bestätigt; SSP 329: +10 bis +30 °C); die vier Relais bilden Stufe 0–9 binär ab. Nicht veröffentlicht: der Widerstand je Stufe und ob mehr Widerstand wärmer oder kälter bedeutet. **R0 und die Segmentwerte erst nach der Messung bestellen.** Ist die Stufe mit grösserem Widerstand die wärmere, ist R0 die kälteste Stufe; sonst R0 und die Abbildung Stufe → Bits umkehren.

### 4.2 Digitaleingänge (SM-I-009, 3–24 V)

Eingangsstufe: Optokoppler mit 2,2 kΩ, typisch 5 mA. I²C-Adresse (0x20 + Stack) XOR 7, Stack 0 = 0x27. Flankenzähler nur auf Hardware V3.

| Kanal | VW-Pin | Signal | Bemerkung |
| --- | --- | --- | --- |
| DI1 | T32b/15 (vi/br → T6bx/5 → J768 T32d/9) | J768 Rückmeldung A | Wahrheitstabelle messen; 4,5/12-V-Pegel sind unbestätigt |
| DI2 | T32b/14 (ge/br → T6bx/6 → J768 T32d/10) | J768 Rückmeldung B | dito |
| DI3 | T32b/9 (ge/rt → T6bj/4) | Kühlbox Diagnose Klemme D | vermutlich Low-Side-Ausgang; Pegel messen. BD35F: Codes 1–5, ca. 250 ms je Puls, 4 s Pause; Code 6 nicht Secop-Standard |
| DI4 | T6ap/2 (ge/gr → J604 T10ba/2) | J604 Betriebsanzeige | Verhalten bei Start, Heizen, Nachlauf messen |
| DI5 | T32b/22 (vi → A11 T6bp/1) | A11 Ladegerät Status | Polarität undokumentiert; evtl. ohne Funktion (O5) |
| DI6 | T32b/3 (ws/vi → B464 → J285) | Geschwindigkeitssignal | Amplitude und Pulse/km messen; Flankenzähler SM-I-009 V3 |
| DI7 | T32b/27 (B341) | Klemme 58d (mit dimmbarer Instrumentenbeleuchtung) | evtl. PWM; Software wertet «hoch im Fenster von 500 ms» als an. Wellenform aufzeichnen |
| DI8 | T32b/28 (B518) | Klemme 86s | bestätigt; Schlüssel ab = aus, gesteckt = an (messen) |
| DI9 | T32b/29 (B478 → J519 T73a/3 und Spule J713) | Laderelais-Befehl / Motor läuft | kein rohes D+; schaltet vermutlich nach Bordspannung, wenn J713 angesteuert wird. Nur loggen (O8) |
| DI10 | T32b/30 | Zündung 15a (SC34) | zusätzlich entkoppelt an Wake-MCU |
| DI11 | T6ap/4 (B340) | 58d Variante ohne dimmbare Beleuchtung | nur falls belegt |
| DI12 | T6ap/6 | 15a Variante (SC32) | Reserve |
| DI13–16 | – | frei | |

Welche 58d-/15a-Kontakte belegt sind, hängt von der Ausstattung ab. PR-Codes vom Datenaufkleber notieren (A8L/A8R; 9CE/9CI ändern Licht- und Ladezweige), bevor Pins 27–30 festgelegt werden.

### 4.3 Analogeingänge (SM-H-001, 0–3,3 V)

**Korrektur gegenüber Version 1.0:** Die Analogeingänge messen 0–3,3 V mit 12 bit, nicht 0–10 V. 0–10 V gilt nur für die vier Analogausgänge (DAC) der Karte.

- Sequent User's Guide v3.0: 0–3,3 V, 12 bit, je Eingang 15 kΩ Pull-up auf der Karte für Thermistoren; mit einem parallelen 0805-Widerstand verkleinerbar.
- Schema: je Kanal 51 Ω Serie, 15 kΩ Pull-up und 220 nF, direkt am STM32. **Keine Klemmdioden, kein Teiler.**
- Händlerangabe V4: max. 3 V, 50 kΩ Eingangsimpedanz. Passt nicht zur v3-Anleitung; der Pull-up kann je Revision anders sein. Auslegung: **max. 3,0 V**.
- Karte prüfen: PCB-Revision, Firmware und Spannung an jedem vorgesehenen AI-Eingang offen gegen Masse notieren. Etwa 3,3 V heisst: Pull-up bestückt.

**Widerstandsgeber (G817, G249, G120, G126):** Geber zwischen AI und Masse, Pull-up von **3,3 V** (der 15-kΩ-Pull-up der Karte oder ein externer Widerstand an die 3,3-V-Schiene der Karte). **Nie von der 5,1-V-Schiene hochziehen:** Ein offener Geber legte sonst 5 V an einen ungeschützten 3,3-V-Eingang. Die Tankgeber brauchen voraussichtlich einen kleineren Pull-up als 15 kΩ für brauchbare Auflösung. G120 ist ein Stufengeber (diskrete Stufen), G126 praktisch binär. Für keinen der vier Geber gibt es verlässliche Widerstandswerte: messen. Die Software rechnet ratiometrisch gegen die gemessene 3,3-V-Schiene der Karte (Register 0x3F); AI5 wird dafür nicht mehr gebraucht.

**12-V-Kanäle (AI6, AI7, AI8):** Teiler auf **≤ 3,0 V**, nicht ≤ 6 V / ≤ 8 V. Beispiel 47 kΩ / 10 kΩ: etwa 2,6 V bei 14,8 V und 2,8 V bei 16 V. Zusätzlich Serienwiderstand und Klemmung auf 3,3 V (z. B. BAT54S), weil die Karte keinen Eingangsschutz hat. Ist der 15-kΩ-Pull-up bestückt, verschiebt er den Teiler: entweder in die Rechnung aufnehmen oder auf diesen Kanälen entfernen. Die Software meldet «Analogeingang über 3,0 V», wenn ein Teilerkanal die Grenze überschreitet.

| Kanal | VW-Pin | Geber / Signal | Messnetzwerk |
| --- | --- | --- | --- |
| AI1 | T32b/7 (ws/br), /8 (gn/br) | G817 NTC Kühlbox | Pull-up 3,3 V; Wert nach Messung (`TODO_MEASURE`) |
| AI2 | T32b/20, /21 | G120 Frischwasser, Stufengeber | Pull-up 3,3 V, kleiner als 15 kΩ; nach Messung |
| AI3 | T32b/18, /19 | G126 Abwasser, voll/nicht voll | dito |
| AI4 | T32b/16, /17 | G249 NTC aussen | Pull-up 3,3 V; nach Messung |
| AI5 | – | frei (Referenz kommt aus der Karte) | – |
| AI6 | T32b/11 | Dachbefehl A (Messmodus) | Teiler 47 k / 10 k + Serie + BAT54S |
| AI7 | T32b/12 | Dachbefehl B (Messmodus) | dito |
| AI8 | T32b/32 (rt/ge 0,5, SH21 5 A) | Batteriespannung, Rückfall ohne Cerbo | dito; Abfall über SH21 direkt an beiden Sicherungsklemmen messen |

**Achtung Messmodus:** Der 15-kΩ-Pull-up sitzt auf der Karte und lässt sich mit den Patchsteckern nicht abtrennen. Parallel zum Original-E153 würden AI1–AI4 die Geberleitungen belasten und die Messung des E153 verfälschen. Für die Aufzeichnung am Original deshalb Multimeter oder Logikanalysator am Breakout verwenden, nicht die Karte.

### 4.4 Pi-5-Header

| GPIO | Funktion | Bemerkung |
| --- | --- | --- |
| 2 / 3 | I²C SDA/SCL → SM-H-001 (0x28), SM-I-009 (0x27), (Touch) | beide Karten nutzen laut Sequent nur I²C (O4 erledigt) |
| 4 / 5 | UART2 TX/RX ↔ XIAO ESP32-S3 | `dtoverlay=uart2-pi5` |
| 17, 27, 22, 23 | H1–H4 (Purecrea) | Polarität `outputs.heater_ladder.gpio_active` prüfen |
| 26 | `ALIVE` → XIAO (high, solange `camperd` läuft) | Power-Good-Ersatz, zusätzlich zur 3,3-V-Schiene |

**Bootzustand H1–H4:** GPIO17/27/22/23 sind nach dem Einschalten Eingänge mit Pull-down. Zieht das Purecrea-Modul bei Low an (typisch für solche Module), würden die Relais bis zum Start von `camperd` anziehen. Deshalb in `config.txt` den Ruhepegel festlegen, z. B. `gpio=17,22,23,27=op,dh` bei Low-aktiv (`=op,dl` bei High-aktiv). Bis die Polarität geprüft ist, steuert `camperd` H1–H4 nicht an.

### 4.5 Steckerlage (VW Einbauseite 805/3, Blick auf die Rückseite von E153)

- T32b: Pin 1 oben links, 16 oben rechts, 17 unten links, 32 unten rechts.
- T6ap: Pin 1 oben links, 3 oben rechts, 4 unten links, 6 unten rechts.
- T2p: senkrecht, Pin 2 über Pin 1.

Bestellnummern der Gehäuse sind daraus nicht ableitbar: Herstellermarkierungen und Codierrippen fotografieren.

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

### 5.3 Netzwerk Pi 5 ↔ Cerbo GX

```
Cerbo GX ── Ethernet ── Camper-Router (unverändert: VRM, Remote Console)
Cerbo GX ── WLAN-AP «venus-…» (immer an) ── Pi 5 wlan0   (MQTT 1883, nur dieser Weg für Victron-Daten)
Pi 5 eth0 ── LAN-Port Camper-Router (Router nur zeitweise an: SSH, Fernzugriff)
```

- Cerbo (Daniel, von Hand): Einstellungen → WLAN → «Access Point erstellen» = EIN; Einstellungen → Dienste → «MQTT im LAN» = EIN (unverschlüsselt 1883 genügt im abgeschlossenen AP-Netz).
- Pi, erstes Mal: `sudo deploy/network/setup-network.sh "<venus-SSID>" "<Passwort vom Etikett>"` (NetworkManager; WLAN nie Standardroute, Ethernet DHCP mit Metrik 100). Prüfen: `python3 tools/cerbo_check.py`. Danach am Gerät änderbar: Expertenansicht → Netzwerk.
- Der Cerbo bridgt/routet nicht zwischen WLAN und Ethernet (Venus-OS-Kernel ohne Bridge). Kein Versuch.
- Keine Steuerfunktion hängt am Router-Netz.
- Firewall auf `wlan0`: keine neuen eingehenden Verbindungen (`deploy/network/nftables.conf`).
- **Rückfall, falls MQTT über den AP nicht antwortet (O19):** kleiner unmanaged Switch, Cerbo + Router + Pi in einem Subnetz, feste Adressen Router .1, Cerbo .10, Pi .20. Etwa 0,5–1,5 W zusätzlich. Erst nach Rückmeldung umbauen.

## 6. Vor dem Einbau messen (Daniel)

1. **J604:** Widerstand T2p je Anzeigestufe (E153-Seite, T2p abgezogen); T6ap/2 und /3 Spannung, Strom und Zeitverlauf (Start, Heizen, Nachlauf)
2. **Kühlbox:** T32b/9 und /10 Spannung, Strom, Zeitverlauf; Kennlinie G817; beide Typenschilder (Kompressor, Elektronik)
3. **Dach:** T32b/11, /12, /14, /15 gleichzeitig in allen vier Dachzuständen (zu, öffnet, offen, schliesst)
4. **Fahrzeug:** T32b/3 Frequenz und Amplitude, /29, /28, beide 58d (Wellenform), beide 15a
5. **Geber:** Kennlinien G120, G126, G249
6. **Typenschilder/Fotos:** A11, MultiPlus, Victron-Monitor (Cerbo-Geräteliste zeigt SmartShunt/BMV und das MultiPlus-Modell), SM-H-001 (PCB-Revision, Firmware, offene AI-Spannungen), alle drei E153-Stecker; PR-Codes vom Datenaufkleber
7. **Bauraum** hinter dem E153-Ausschnitt: Breite, Höhe, Tiefe, Biegeradien, Halterungen (keine veröffentlichten Masse; max. 18 cm Breite laut Daniel)
8. Purecrea-Modul: Eingangspegel, der ein Relais anzieht (Low oder High), bei 3,3-V-Ansteuerung

Hilfreich: das Werkstattmenü des Original-E153 (Escape > 1 s halten, Drehknopf drücken) zeigt Rohwerte; gleichzeitig am abgezogenen Geber messen.
