# Cali Camping Unit – Hardware

Version 2.1 | 1. November 2026 | Ersetzt Version 1.0 (Pi 5 + XIAO). Änderung gegenüber 2.0: Viewe-Board und Viewe-MIPI-Display statt Waveshare + Newhaven, keine eigene Adapterplatine. Grundlage: Pin-Check Nr. 133, Hardware 1.0, Verkabelungsplan Entwurf 0.1

Preise sind Budgetwerte in CHF, keine Offerten. Status «Bestätigen/Anfragen/Prüfen» heisst: vor der Bestellung klären.

## 0. Architekturentscheid 1.11.2026: ESP32-P4 regelt und zeigt an

Ein ESP32-P4 übernimmt Regelung, Anzeige und Victron-Anbindung. Pi 5, XIAO ESP32-S3, DCDC-USB, HDMI-Treiberplatine und die UART-Verbindung Pi ↔ XIAO entfallen. Board und Display kommen beide von Viewe und werden mit einem FPC-Kabel verbunden.

| Thema | Entscheid |
| --- | --- |
| Rechner | Viewe ESP32-P4-Pi-VIEWE (P4 + ESP32-C6 für WLAN, Ethernet, 2-Lane-DSI, 40-pol. Header in Pi-Belegung, 85 × 56 mm). Läuft dauernd. |
| Regelung | Der P4 ist einziger Master für alle Ausgänge: Kühlschrank-Thermostat, Heizung (S+, Sollwertleiter), Dach inkl. Freigabe Zündung + Stillstand. |
| Anzeige | Viewe 6,8" 480 × 1280, MIPI-DSI-Version mit kapazitivem Touch über I²C. Dasselbe Panel wie die bisher geplante HDMI-Version, Aussenmass 181 × 66,6 mm. Ein FPC führt Bild, Touch und Versorgung. Init-Sequenz in Viewes ESP-IDF-Komponente `esp_lcd_dsi`. Bestellnummer und 2-Lane-Betrieb bei Viewe angefragt (O15). |
| Wecken | Touch-INT am P4. Display und Hintergrundbeleuchtung aus, wenn nicht bedient; Touch bleibt versorgt. Erste Berührung quittiert der Summer, sie löst keine Bedienung aus. |
| Zündung | über DI10 des SM-I-009 (Optokoppler). Kein 12-V-Signal direkt an einen GPIO. |
| I²C | Bus 0: SM-H-001 und SM-I-009. Bus 1: Touch (über das Display-FPC). P4 ist auf beiden einziger Master. |
| Sequent-Karten | Abgesetzt wie bisher. Option: SM-H-001 direkt auf den Pi-kompatiblen Header stecken; eine Rückspeisung wie beim Pi entfällt, weil Board und Karten an derselben Dauerschiene hängen (O20). |
| Software | ESP-IDF (C), Oberfläche in LVGL nach Vorlage `Camping_Unit_Oberflaeche_v2.html`. `SOFTWARE_SPEC_v1_0.md` muss neu geschrieben werden. |
| Netzwerk | WLAN (C6) am Access Point des Cerbo für MQTT; Ethernet des P4 am LAN-Port des Camper-Routers für Fernzugriff, wenn der Router an ist. |

**Folgen:**
- Keine eigene Platine für die Anzeige. Die Versorgung der Hintergrundbeleuchtung kommt laut Viewe über das FPC; Dimmen und Abschalten per PWM vom P4 ist angefragt (O15).
- Rückfall, falls Viewe den 2-Lane-Betrieb nicht bestätigt: Newhaven NHD-6.8 mit eigener Adapterplatine (Anhang A) oder Waveshare 7,9" (207,8 × 69,8 mm, grössere Frontblende).
- Die Oberfläche wird in LVGL neu gebaut; die React-Oberfläche entfällt. Fernzugriff nur über eine einfache Weboberfläche auf dem P4.
- **Ruhestrom:** Ziel bleibt geparkt mit aktiver Regelung unter 1 W. Der P4 mit dauernd aktivem WLAN liegt ohne Stromsparmodus darüber. Light Sleep des P4 und Modem Sleep des C6 sind dafür Pflicht. Auf dem Prüfstand messen, bevor der Einbau festgelegt wird.

## 1. Übersicht

```
12 V Zweitbatterie ── F1 7,5 A ── Verpolschutz ── TVS ──┬── DC/DC 5 V 3 A (immer an) ──┬── ESP32-P4-Pi-VIEWE ── FPC (DSI, Touch, Versorgung) ── Viewe 6,8" MIPI
                                                       │                              ├── SM-H-001, SM-I-009 (I²C 0)
                                                       │                              └── Relaismodul H1–H4
                                                       └── F2 2 A ── COM K1–K4
Fahrzeug T32b / T6ap / T2p ── Breakout X1–X5 ── SM-H-001 (K1–K4, AI1–AI8) / SM-I-009 (DI1–DI16) / H1–H4
P4 WLAN (C6) ── Access Point Cerbo GX (MQTT)
P4 Ethernet ── LAN-Port Camper-Router (nur wenn Router an)
Touch ── I²C 1 + INT über FPC ── P4
```

## 2. Bereits im Fahrzeug (nicht kaufen)

| Gerät | Rolle | Zu prüfen |
| --- | --- | --- |
| Victron Cerbo GX | Datenquelle Batterie, Laden, 230 V über MQTT | MQTT einschalten; Access Point an lassen; MQTT über den Access Point testen (O10) |
| Camper-Router (WLAN) | Fernzugriff, nur zeitweise an | Modell und freier LAN-Port |
| Victron MultiPlus Compact (Annahme 12/1600/70) | Wechselrichter, Ladegerät, Netzumschaltung | Typenschild |
| Batteriemonitor (SmartShunt/BMV?) | SoC | vorhanden? Ohne Shunt kein SoC |
| J768, J698/J1029, J604, G817, G249, G120, G126 | VW-Originalgeräte, bleiben | – |
| A11 VW-Ladegerät | evtl. durch MultiPlus ersetzt | eingebaut oder nicht (O5) |
| N162 VW-Shunt | wird nicht mehr gebraucht | T32b/25, /26 nur auf Breakout |

## 3. Stückliste

| Pos | Gruppe | Bauteil | Modell | Menge | Status | CHF/Stk | CHF | Funktion | Hinweis |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Rechner | ESP32-P4-Board mit C6, Ethernet, 2-Lane-DSI | Viewe ESP32-P4-Pi-VIEWE | 1 | Bestellen | 18.00 | 18.00 | Regelung, Anzeige, Netzwerk | 17.99 USD; Schaltpläne auf GitHub |
| 3 | Rechner | microSD 32 GB High Endurance | A1 | 1 | Bestätigen | 18.00 | 18.00 | Messdaten, Ereignislog | Kartenslot am Board prüfen |
| 4 | Rechner | RTC-Batterie oder -Akku | passend zum Board | 1 | Bestätigen | 6.00 | 6.00 | Uhrzeit ohne Netz | RTC am Board prüfen |
| 5 | Anzeige | 6,8" 480×1280 IPS, MIPI DSI, CTP über I²C | Viewe, Bestellnummer angefragt | 1 | Anfragen | 45.00 | 45.00 | Anzeige und Bedienung | nicht die -HMD-H-Version |
| 6 | Anzeige | FPC- oder Zwischenkabel Board ↔ Display | Viewe | 1 | Anfragen | 5.00 | 5.00 | Bild, Touch, Versorgung | Länge nach Einbau |
| 11 | I/O | Home Automation HAT | Sequent SM-H-001 | 1 | Geplant | 80.00 | 80.00 | K1–K4, AI1–AI8 | abgesetzt, nicht gesteckt |
| 12 | Strom | DC/DC 12 → 5 V, 3 A, kleiner Ruhestrom | Pololu D36V28F5 (Vorschlag) | 1 | Prüfen | 30.00 | 30.00 | 5 V Dauer | Ruhestrom und Wirkungsgrad bei kleiner Last prüfen |
| 13 | I/O | 16 LV-Digitaleingänge | Sequent SM-I-009 | 1 | Geplant | 60.00 | 60.00 | DI1–DI16 (3–24 V) | Adresse 0x27 (Level 0) |
| 14 | Heizung | 4-fach-Relaismodul, getrennte COM/NC/NO | Purecrea 420053 | 1 | Geplant | 12.90 | 12.90 | H1–H4 Sollwertleiter | Einschaltpegel bei 3,3-V-Logik prüfen |
| 15 | Bedienung | Summer 5 V + Transistor | Piezo aktiv | 1 | Bestellen | 4.00 | 4.00 | Quittung Berührung | |
| 16 | Heizung | Präzisionswiderstand Basis der Sollwertleiter | Metallfilm 0,1 % | 1 | **Nicht bestellen** | 4.00 | 4.00 | Basis der Sollwertleiter | Wert erst nach Messung T2p je Stufe (O23). Der frühere Wert 1750 Ω ist ein unvalidierter Entwurf |
| 17 | Heizung | Präzisionswiderstände Segmente H1–H4 | Metallfilm 0,1 % | 1 | **Nicht bestellen** | 8.00 | 8.00 | Segmente H1–H4 | dito; 27,4 / 54,9 / 110 / 237 Ω gehören zum unvalidierten Entwurf |
| 18 | Fahrzeug | Gegenstecker T32b | VW, Codierung am E153 ablesen | 1 | Bestätigen | 35.00 | 35.00 | Originalkabelbaum | Teilenummer nach Gehäusefoto |
| 19 | Fahrzeug | MQS-Kontakte/Reparaturleitungen | 32 + 6 Reserve | 38 | Bestellen | 2.20 | 83.60 | alle 32 Pins | |
| 20 | Fahrzeug | Gegenstecker T6ap komplett | VW | 1 | Bestätigen | 18.00 | 18.00 | Heizung, Zündung | |
| 21 | Fahrzeug | Gegenstecker T2p komplett | VW | 1 | Bestätigen | 10.00 | 10.00 | Heizungssollwert | |
| 22 | Breakout | 8-pol. Panel-Durchführung | Phoenix DFK-MSTB 2,5/8 | 5 | Bestellen | 22.00 | 110.00 | X1–X4 T32b, X5 T6ap + T2p | X5 neu |
| 23 | Breakout | 8-pol. Gegenstecker | Phoenix MSTB 2,5/8-STF-5,08 | 10 | Bestellen | 10.00 | 100.00 | 5 innen, 5 aussen | |
| 24 | Verdrahtung | FLRY-B 0,35 mm², farbig | Meter | 25 | Bestellen | 1.20 | 30.00 | Signalleitungen | |
| 25 | Verdrahtung | FLRY-B 1,0 mm² rot/braun | Meter | 5 | Bestellen | 2.00 | 10.00 | Versorgung, Masse | |
| 26 | Verdrahtung | Adermarker 01–32 + Schrumpfschlauch | Set | 1 | Bestellen | 22.00 | 22.00 | Beschriftung beidseitig | |
| 27 | Verdrahtung | Micro-Fit-Serviceverbinder 12-pol. | Molex | 3 | Bestellen | 12.00 | 36.00 | Module trennbar | |
| 28 | Strom | ATO-Sicherungshalter + 7,5 A | Inline | 1 | Bestellen | 12.00 | 12.00 | 12-V-Zuleitung Unit (F1) | eigene Zuleitung, nicht über SH21 |
| 29 | Strom | Lastabwurf-TVS | Littelfuse SLD15-018 | 1 | Abstimmen | 22.00 | 22.00 | 12-V-Eingang | Klemmspannung mit Wandler abstimmen |
| 30 | Strom | Verpolschutz 10 A | Ideal-Diode-Modul | 1 | Bestätigen | 35.00 | 35.00 | 12-V-Eingang | |
| 36 | Service | Panel-USB-C-Verlängerung | mit Kappe | 1 | Bestellen | 18.00 | 18.00 | Flashen, Service | |
| 41 | I/O | I²C-Leitung zu beiden Sequent-Karten | LiYCY 4×0,25 mm² + 2×20-Adapter | 1 | Bestätigen | 15.00 | 15.00 | I²C 0 | max. ca. 30 cm; Pull-ups 2,2 kΩ |
| 42 | Netz | Patchkabel Cat 6, flach | Länge nach Einbau | 1 | Bestellen | 8.00 | 8.00 | P4 ↔ Camper-Router | |
| 43 | Messung | Messnetzwerk: Pull-ups, Teiler, BAT54S | Metallfilm 0,1 %, Sortiment | 1 | Bestellen | 12.00 | 12.00 | AI1–AI8 | |
| 44 | Strom | ATO-Sicherungshalter + 2 A | Inline | 1 | Bestellen | 6.00 | 6.00 | COM K1–K4 (F2) | |
| | | **Zwischensumme** | | | | | **873.50** | | |
| | | Reserve 15 % | | | | | 131.03 | | |
| | | **Planungsbudget** | | | | | **1'004.53** | | |

Entfallen gegenüber 1.0: Raspberry Pi 5, Active Cooler, HDMI-Treiberplatine, separater Touch, Micro-HDMI-Kabel, DCDC-USB, Pololu D36V6F5, XIAO ESP32-S3. Entfallen gegenüber 2.0: Waveshare ESP32-P4-NANO, Newhaven-Display, DSI-Adapterplatine, LED-Treiber, 3,3-V-Regler, Molex-Stecker.

Noch nicht budgetiert (Menge erst nach Bauraumaufnahme): Pos. 31 Aluträger, 32 Gehäuse, 33 Frontblende für den E153-Ausschnitt, 34 Abstandshalter, 35 EPDM-Dichtung, 37 Testbuchsen, 38 Kabelschutz, 39 Lötstützpunkte, 40 MQS-Crimpwerkzeug. Richtwert zusammen rund CHF 400.

### Prüfstand (für Entwicklung, nicht im Fahrzeug)

| Teil | Zweck | CHF |
| --- | --- | --- |
| Labornetzteil 0–15 V / 5 A | Unit am Tisch, Unterspannung testen | vorhanden? |
| Prüfbrett: 8 Kippschalter (12 V), 4 Potis 0–2 kΩ, 4 LEDs mit Vorwiderstand | DI, Tankgeber/NTC und K1–K4 nachbilden | 25 |
| USB-Logikanalysator 8 Kanäle (optional) | Pulse und Wahrheitstabelle mitschreiben | 20 |
| USB-Leistungsmesser oder Shunt-Messung 12 V | Ruhestrom messen | 25 |

## 4. Kanalbelegung

Quelle der Wahrheit für die Software ist `profiles/t6-california-a8l-2018.json`. Diese Tabelle muss damit übereinstimmen. Die Fahrzeugseite ist unverändert gegenüber 1.0.

### 4.1 Ausgänge

| Kanal | Karte | VW-Pin | Gerät | Schaltet | Bemerkung |
| --- | --- | --- | --- | --- | --- |
| K1 | SM-H-001 Relais 1 | T32b/11 | J768 Dach Befehl A | +12 V über COM (Annahme) | Richtung und Pegel messen (O6) |
| K2 | SM-H-001 Relais 2 | T32b/12 | J768 Dach Befehl B | +12 V | dito |
| K3 | SM-H-001 Relais 3 | T32b/10 | J1029 Kühlanforderung (Klemme K4) | **unbekannt** (+12 V war Entwurfsannahme) | Ausgangsart erst nach Messung und Elektronik-Datenblatt (`docs/KUEHLBOX.md`) |
| K4 | SM-H-001 Relais 4 | T6ap/3 | J604 S+ | +12 V | Zuordnung zu T6ap/2 messen |
| K5–K8 | SM-H-001 Relais 5–8 | – | frei | – | nicht für Sollwertleiter (gemeinsame COM) |
| H1 | Relaismodul IN1, P4-GPIO | T2p | Segment, Wert offen (O23) | NC überbrückt | Relais an = Segment wirksam |
| H2 | Relaismodul IN2, P4-GPIO | T2p | Segment, Wert offen (O23) | NC | |
| H3 | Relaismodul IN3, P4-GPIO | T2p | Segment, Wert offen (O23) | NC | |
| H4 | Relaismodul IN4, P4-GPIO | T2p | Segment, Wert offen (O23) | NC | Werte erst nach Messung |

**COM K1–K4:** Die vier Relais teilen sich eine COM. Annahme: alle schalten +12 V über F2 (2 A). Zeigt die Messung, dass ein Eingang nach Masse geschaltet werden muss, braucht dieser Kanal ein Relais aus der zweiten Gruppe (K5–K8) mit COM auf Masse.

**Sollwertleiter (T2p/1–T2p/2, 10 Stufen):** Belegte Quellenwerte nur Stufe 1 = 333 Ω, Stufe 5 = 839 Ω, Stufe 10 = 1993 Ω (Tristan, Pekaway-Forum, Abschnitt «Eberspächer», https://forum.pekaway.de/t/webasto-und-der-w-bus-erklart-und-viele-gerate-implementieren/1897; kein Nachweis für dieses Fahrzeug). Die Zwischenstufen 2–4 und 6–9 (ca. 439, 558, 690, 1008, 1201, 1424, 1685 Ω) sind frühere, nicht belegte Schätzungen und werden nicht verwendet. **Unvalidierter anderer Entwurf:** Leiter 1750 Ω + Segmente 27,4 / 54,9 / 110 / 237 Ω (1750–2179,3 Ω, 16 Kombinationen); passt nicht zum Bereich 333–1993 Ω und ist keine Zehnstufen-Tabelle. Leiter-Entwurf erst nach Messung aller zehn Stufen am E153-seitigen T2p (`docs/MESSBLATT.md` 2): Zielwiderstand und tatsächlich erzeugten Widerstand je Stufe abgleichen. Stromlos überbrücken die NC-Kontakte alle Segmente.

### 4.2 Digitaleingänge (SM-I-009, 3–24 V, Adresse 0x27)

| Kanal | VW-Pin | Signal | Bemerkung |
| --- | --- | --- | --- |
| DI1 | T32b/15 | J768 Rückmeldung A | |
| DI2 | T32b/14 | J768 Rückmeldung B | |
| DI3 | T32b/9 | Kühlbox Diagnose Klemme D | Polarität und Protokoll unbekannt («low-aktiv» ist Hypothese); vorerst nur Rohaufzeichnung |
| DI4 | T6ap/2 | J604 Betriebsanzeige | |
| DI5 | T32b/22 | A11 Ladegerät Status | evtl. ohne Funktion (O5) |
| DI6 | T32b/3 | Geschwindigkeitssignal J285 | Flankenzähler der Karte (Register 25 ff.) statt Abfrage; löst O14 |
| DI7 | T32b/27 | Klemme 58d (dimmbare Instrumentenbeleuchtung) | |
| DI8 | T32b/28 | Klemme 86s | |
| DI9 | T32b/29 | J519 T73a/3, Hypothese D+ | messen |
| DI10 | T32b/30 | Zündung 15a (SC34) | einziger Zündungseingang des P4 |
| DI11 | T6ap/4 | 58d Variante ohne dimmbare Beleuchtung | nur falls belegt |
| DI12 | T6ap/6 | 15a Variante (SC32) | Reserve |
| DI13–16 | – | frei | |

### 4.3 Analogeingänge (SM-H-001, max. 3,0 V)

Die Analogeingänge des SM-H-001 messen 0–3,3 V, laut V4-Händlerangabe max. 3 V. Keine Schutzdioden; v3 hat pro Kanal einen eingebauten 15-kΩ-Pull-up auf 3,3 V. Pull-ups nur von 3,3 V, alle 12-V-Teiler auf ≤ 3,0 V (z. B. 47 kΩ / 10 kΩ), Serienwiderstand plus Klemmdiode auf 3,3 V (BAT54S). Platinenversion und eingebauten Pull-up vor der Auslegung prüfen.

| Kanal | VW-Pin | Geber | Messnetzwerk |
| --- | --- | --- | --- |
| AI1 | T32b/7 (A), /8 (B) | G817 Kühlbox-Fühler (Typ und Kennlinie unbekannt) | Pull-up nach Messung (`TODO_MEASURE`) |
| AI2 | T32b/20, /21 | G120 Frischwasser, 4 Stufen | Pull-up nach Messung der Stufenwiderstände |
| AI3 | T32b/18, /19 | G126 Abwasser, voll/nicht voll | dito |
| AI4 | T32b/16, /17 | G249 NTC aussen | dito |
| AI5 | – | Referenz 3,3-V-Schiene (ratiometrisch) | Teiler auf ≤ 3,0 V |
| AI6 | T32b/11 | Dachbefehl A (Messmodus) | Teiler 16 V → ≤ 3,0 V + Klemmdiode |
| AI7 | T32b/12 | Dachbefehl B (Messmodus) | dito |
| AI8 | T32b/32 | Batteriespannung, Rückfall ohne Cerbo | dito |

**Achtung Messmodus:** Solange das Original-E153 angeschlossen ist, keine Pull-ups an AI1–AI4. Im Messmodus die Pull-ups über die Patchstecker abtrennen und nur hochohmig mithören.

### 4.4 GPIO-Budget ESP32-P4

GPIO-Nummern nach dem Pinout des ESP32-P4-Pi-VIEWE festlegen. Welche Display-Signale das Board fest auf den DSI-Stecker legt, steht im Viewe-Schaltplan.

| Funktion | Richtung | Ziel | Bemerkung |
| --- | --- | --- | --- |
| I2C0 SDA, SCL | bidir. | SM-H-001, SM-I-009 | Pull-ups 2,2 kΩ auf 3,3 V |
| I2C1 SDA, SCL | bidir. | Touch | über das Display-FPC |
| Touch INT | ein | Touch | Pin, der aus Light Sleep weckt; über FPC verfügbar? (O15) |
| Touch RST | aus | Touch | |
| Display Reset | aus | Panel | meist vom Board fest belegt |
| PWM Hintergrundbeleuchtung | aus | Panel | Steuerung laut Viewe (O15) |
| H1–H4 | aus | Relaismodul | Ruhezustand stromlos |
| Summer | aus | Transistor | |
| SM-I-009 INT | ein | Karte | optional, Pin laut Schaltplan |
| **Summe** | | | **ca. 14 Pins** plus DSI |

## 5. Stromversorgung

### 5.1 Leistungsbudget 5 V

| Verbraucher | typ. A | Spitze A | Bemerkung |
| --- | --- | --- | --- |
| ESP32-P4-Pi-VIEWE inkl. C6 | 0,25 | 0,60 | mit Stromsparmodus tiefer |
| SM-H-001 | 0,15 | 0,60 | 8 Relais an |
| SM-I-009 | 0,05 | 0,10 | |
| Relaismodul H1–H4 | 0,00 | 0,30 | |
| Display inkl. Hintergrundbeleuchtung | 0,35 | 0,50 | Schätzung; Datenblatt der MIPI-Version abwarten. Nur wenn Display an |
| Touch | 0,01 | 0,02 | dauernd versorgt |
| Summer | 0,00 | 0,03 | |
| **Gesamt** | **0,81** | **2,15** | Display an |

Geparkt mit dunklem Display ohne angezogene Relais: P4, Touch, Karten. Schätzung 0,3–0,5 A an 5 V (1,5–2,5 W) ohne Stromsparmodus. Mit Light Sleep und Modem Sleep Ziel unter 1 W. Auf dem Prüfstand messen.

### 5.2 Regeln

- Eigene 12-V-Zuleitung mit 7,5 A (F1). T32b/32 (über SH21, 5 A, 0,5 mm²) nur als Messpunkt.
- Alle 12-V-Signale nie direkt an einen GPIO. Zündung über DI10 des SM-I-009.
- Masse der Unit an T32b/31 und T6ap/5 und an die Zuleitungsmasse, sternförmig.
- Das P4-Board über die 5-V-Pins des Headers speisen (laut Schaltplan bestätigen) oder über USB-C, nicht beides gleichzeitig. USB-C nur für Service bei abgeklemmter Unit oder mit Diode.

### 5.3 Netzwerk

- Ethernet-Port des Cerbo bleibt am Camper-Router. Fernzugriff auf den Cerbo (VRM, Remote Console) wie heute.
- P4 ↔ Cerbo über WLAN (ESP32-C6) am Access Point des Cerbo. Der Access Point ist immer an.
- P4-Ethernet am LAN-Port des Routers für Fernzugriff auf die Weboberfläche, wenn der Router an ist.
- Bluetooth des C6 aus.
- **O10:** Prüfen, ob MQTT über den Access Point antwortet. Falls nicht: kleiner ungemanagter Switch (Cerbo, Router, P4), Mehrverbrauch 0,5–1,5 W.

## 6. Anzeige-Anschluss

Ein FPC-Kabel vom DSI-Stecker des ESP32-P4-Pi-VIEWE zum Display. Es führt Bild (2 Lanes), Touch (I²C, INT) und Versorgung. Keine eigene Platine.

Bei Viewe angefragt (O15):
1. Bestellnummer der MIPI-Version mit Touch, Preis
2. 2-Lane-Betrieb am ESP32-P4-Pi-VIEWE
3. Passt das FPC direkt oder braucht es ein Zwischenkabel
4. Versorgung und PWM-Dimmung der Hintergrundbeleuchtung, ganz abschaltbar
5. Touch-Controller, INT über das FPC, Ruhestrom
6. ESP-IDF-Komponente mit Init-Sequenz
7. Datenblatt und Zeichnung der MIPI-Version mit FPC-Belegung

Rückfall bei negativer Antwort: Newhaven NHD-6.8 mit Adapterplatine nach Anhang A.

## 7. Vor dem Einbau messen (Daniel)

1. Widerstand T2p am Original-Panel bei Minimum und Maximum
2. Pegel und Polarität an T32b/10, /11, /12 und T6ap/3 im Betrieb
3. Wahrheitstabelle T32b/14 und /15 beim Öffnen und Schliessen
4. T32b/29 bei Motor aus und an (D+?)
5. Widerstände G120 je Stufe, G126 leer/voll, Kennlinien G817 und G249
6. Typenschild Kühlbox-Kompressor und -Elektronik
7. Typenschild MultiPlus, Batteriemonitor vorhanden?
8. Bauraum hinter dem E153-Ausschnitt (max. 18 cm Breite)
9. MQTT über den Access Point des Cerbo (O10); Router-Modell und freier LAN-Port
10. Zuordnung T6ap/2 und /3 (S+ oder Betriebsanzeige)

## 8. Offene Punkte

| Nr. | Frage | Standard bis zur Klärung |
| --- | --- | --- |
| O5 | Ist A11 noch eingebaut? | DI5 nur loggen |
| O6 | Wahrheitstabelle Dach, Pegel aller Ein-/Ausgänge | aus Messmodus |
| O7 | Kennlinien G817, G249, G120, G126, Heizungs-Sollwert | `TODO_MEASURE` |
| O8 | D+ an T32b/29? | nur loggen |
| O10 | MQTT über Access Point des Cerbo | Rückfall Switch |
| O14 | Geschwindigkeitssignal | Flankenzähler SM-I-009 (gelöst, am Fahrzeug bestätigen) |
| O15 | Viewe: Bestellnummer MIPI-Version, 2-Lane-Betrieb, FPC, Hintergrundbeleuchtung, Touch-INT, Init-Sequenz | Anfrage gesendet; Rückfall Newhaven + Adapter |
| O16 | Belegung DSI-Stecker und 5-V-Einspeisung ESP32-P4-Pi-VIEWE | Viewe-Schaltplan auf GitHub |
| O17 | Brauchen die Sequent-Karten 3,3 V vom Header? Eigene Pull-ups auf SDA/SCL? Adresse SM-H-001, Interrupt-Pin SM-I-009 | Schaltpläne Sequent |
| O18 | Schaltet das Relaismodul sicher mit 3,3-V-Logik? | am Tisch prüfen; sonst Transistorstufe |
| O19 | Ruhestrom P4 mit WLAN im Parkbetrieb | Prüfstand; Ziel unter 1 W |
| O20 | SM-H-001 direkt auf den Header des P4-Boards stecken? Belegt die Karte Pins, die das Board anders nutzt? | abgesetzt montieren, bis geklärt |

## Anhang A: Rückfall Newhaven NHD-6.8 mit DSI-Adapterplatine

Nur falls Viewe den 2-Lane-Betrieb nicht bestätigt. Die Platine setzt den DSI-Stecker des Boards auf den 40-poligen FFC des Newhaven um (Molex 54132-4062) und erzeugt die Versorgung: 3,3 V mit Enable für VDD (55 mA typ.) und einen Boost-LED-Treiber 18 V / 170 mA mit PWM (z. B. TI TPS61169, Messwiderstand ca. 1,2 Ω). Zusätzlicher Bedarf an 5 V mit Display an: ca. 0,8 A.

| Newhaven-Pin | Signal | Anschluss |
| --- | --- | --- |
| 1 / 2 | LED_K / LED_A | LED-Treiber |
| 4 | VDD 3,3 V | Regler, Enable vom P4 |
| 6 / 7 | MIPI D0− / D0+ | Board-DSI |
| 9 / 10 | MIPI D1− / D1+ | Board-DSI |
| 12 / 13 | MIPI CLK− / CLK+ | Board-DSI |
| 5, 8, 11, 14, 23, 26, 29, 32, 35, 38 | GND | Massefläche |
| 19 | TE | optional an GPIO |
| 20 | RESX (aktiv low) | GPIO, 10 kΩ Pull-down |
| 21 | IM0 | GND; 3,3 V tauscht alle Paare |
| 22 | BIST_EN | GND |
| 15–18, LVDS, 3, 39, 40 | SPI, LVDS, NC, VPP_OTP | offen |

Touch-Stecker (6-pol., 1,0 mm, Molex 52271-0679): 1 VDD (3,3 V dauernd), 2 GND, 3 SCL, 4 SDA (je 4,7 kΩ Pull-up), 5 /INT, 6 /RESET. Init-Code für den NV3051F-L mit 2 Lanes bei Newhaven anfordern.

Layout: 4 Lagen, impedanzkontrolliert, durchgehende Massefläche, DSI-Paare 100 Ω differenziell, längengleich, unter 5 cm, Boost-Treiber räumlich getrennt.
| O21 | Kühlbox: Elektronik-Typ, Pegel T32b/9 und /10, G817-Kennlinie, Sollbereich und Hysterese | nur Simulation; `docs/KUEHLBOX.md` |
| O22 | Ausgangsart K1–K4 (+12 V, Masse, Open-Drain, potentialfreier Kontakt) | `drive: TODO_MEASURE`, Kanal schaltet nicht |
| O23 | Heizungs-Sollwert: zehn Stufen am T2p messen, Regelrichtung, Leiter-Entwurf | Leiter stromlos, Pos. 16/17 nicht bestellen |
