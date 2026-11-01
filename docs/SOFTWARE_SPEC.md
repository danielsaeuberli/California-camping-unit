# Cali Camping Unit – Software-Spezifikation

Version 2.0 | 1. November 2026 | Fahrzeug: VW T6 California 2.0 TDI 4Motion, MJ 2018, Campingausrüstung A8L/A8R

Ersetzt Version 1.0 (Raspberry Pi 5, Python, React, Wake-MCU). Neu: eine Firmware auf einem ESP32-P4 in C mit ESP-IDF, Oberfläche in LVGL. Fachlogik, Sicherheitsregeln, Messmodus und Oberflächenkonzept bleiben inhaltlich gleich.

Hardware: `docs/HARDWARE.md` (= `HARDWARE_v2_0.md`, Version 2.1). Pinbelegung und Kalibrierwerte: `profiles/t6-california-a8l-2018.json`.

---

## 1. Umfang

Die Unit ersetzt das Bedienteil E153. Die Original-Steuergeräte bleiben im Fahrzeug und werden nur angesteuert:

| Gerät | VW-Bezeichnung | Was die Unit tut |
| --- | --- | --- |
| Dachhydraulik | J768 | Fahranforderung AUF/ZU (K1/K2), Rückmeldungen lesen |
| Kühlbox | J698 / J1029, Secop BD35F | Kühlanforderung (K3), Thermostat auf G817, Fehlercodes lesen |
| Standheizung | J604, Eberspächer Airtronic | Ein/Aus über S+ (K4), Sollwert über Widerstandsleiter (H1–H4), Betriebsanzeige lesen |
| Tanks | G120 Frisch, G126 Abwasser | Stufen anzeigen |
| Temperaturfühler | G817 Kühlbox, G249 aussen | anzeigen |
| Victron | Cerbo GX, MultiPlus Compact 12/1600 | Batterie, Laden, 230 V anzeigen (nur lesen) |
| Fahrzeug | 15a, 86s, D+, 58d, Geschwindigkeit | Betriebszustände, Fahrsperre, Nachtmodus |

**Nicht im Umfang v2:** CAN/LIN zum Fahrzeug, Zugriff aus dem Internet, Schreiben auf den Cerbo, Licht-Dimmer, Batterie-Management.

---

## 2. Plattform

### 2.1 Hardware (Kurzfassung)

| Teil | Modell | Anbindung |
| --- | --- | --- |
| Rechner | Viewe ESP32-P4-Pi-VIEWE (ESP32-P4 + ESP32-C6) | – |
| Anzeige | Viewe 6,8" 480 × 1280, MIPI DSI 2 Lanes, CTP | DSI über FPC; Touch I²C 1 + INT über FPC |
| Ausgänge, Analog | Sequent SM-H-001 | I²C 0 |
| Digitaleingänge | Sequent SM-I-009, Adresse 0x27 | I²C 0 |
| Heizungs-Sollwert | 4-fach-Relaismodul H1–H4 | 4 GPIO |
| Summer | aktiv, über Transistor | 1 GPIO |
| Victron | Cerbo GX | WLAN (C6) am Access Point des Cerbo, MQTT |
| Service | Router im Camper | Ethernet des Boards |

Noch offen bei Viewe (Hardware O15): Bestellnummer der MIPI-Version, 2-Lane-Betrieb, Hintergrundbeleuchtung per PWM, Touch-INT über das FPC, Init-Sequenz. Rückfall: Newhaven NHD-6.8 (Treiber NV3051F-L, Touch FT5426G). Der Display-Treiber muss deshalb hinter einer eigenen Schnittstelle liegen (Kapitel 3).

### 2.2 Werkzeuge

- ESP-IDF, aktuelle stabile Version mit ESP32-P4-Unterstützung. Beim Aufsetzen festlegen und in `README.md` und `.devcontainer` oder `tools/idf-version` pinnen.
- C11, keine dynamische Speicherverwaltung im Regelpfad nach dem Start.
- LVGL 9 über den ESP-Komponentenmanager (`idf_component.yml`, Versionen gepinnt).
- WLAN des C6 über `esp_wifi_remote` / `esp_hosted` (wie in den Espressif-Beispielen für P4 + C6).
- MQTT: `esp-mqtt`. Webserver: `esp_http_server`. Dateisystem: FAT auf microSD für Messdaten, LittleFS in Flash für Profil und Einstellungen.
- Host-Build für Tests und Simulator (Kapitel 3.4).

### 2.3 Ziele

| Grösse | Ziel |
| --- | --- |
| Kaltstart bis bedienbare Oberfläche | ≤ 2 s |
| Berührung bei dunklem Display bis bedienbar | ≤ 300 ms |
| Regelzyklus | DI 20 ms, AI 100 ms, Module 50 ms |
| Ruhestrom geparkt, Display aus, Regelung aktiv | Ziel < 1 W an 12 V (messen, Hardware O19) |
| Flash-Schreibzugriffe | nur bei Änderung von Einstellungen und Profil |

---

## 3. Architektur

### 3.1 Übersicht

```
Fahrzeug ─ Breakout X1–X5 ─ SM-H-001 / SM-I-009 / H1–H4
                                   │ I²C 0, GPIO
Cerbo GX ─ WLAN (C6), MQTT ─┐      │
Touch ─ I²C 1 + INT ───────┐│      │
                           ▼▼      ▼
            Firmware (ESP-IDF, FreeRTOS)
              hal → signals → modules → state_bus
              ui (LVGL) · webui (HTTP) · measure (SD) · power
                           │ DSI
                           ▼
                    Viewe 6,8", quer 1280 × 480
```

### 3.2 Komponenten (`components/`)

| Komponente | Aufgabe |
| --- | --- |
| `hal` | Schnittstelle `hal_io_t` mit `read_di()`, `read_ai()`, `write_relay(ch,on)`, `write_ladder(bits)`, `read_edges(ch)`. Implementierungen: `hal_sim`, `hal_sequent` (SM-H-001 + SM-I-009 über I²C 0), `hal_ladder` (GPIO H1–H4). |
| `display` | Schnittstelle `display_t` mit `init()`, `on()`, `off()`, `set_brightness(pct)`. Implementierungen: `display_viewe68`, Rückfall `display_nhd68`, und `display_sdl` für den Host. |
| `touch` | Treiber hinter `touch_t`, INT als Weckquelle. |
| `profile` | Lädt und validiert `profile.json` (Standard im Flash, Änderungen in LittleFS). Unbekannte Schlüssel → Fehler. Wert `"TODO_MEASURE"` → «nicht kalibriert». |
| `signals` | Rohwert → Wert mit Zeitstempel und Qualität: `ok`, `stale`, `open`, `short`, `implausible`, `uncalibrated`. Entprellen, Median, Flanken, Frequenz. |
| `modules` | Zustandsmaschinen `roof`, `fridge`, `heater`, `tanks`, `climate`, `vehicle`, `energy`. Reines C ohne I/O und ohne FreeRTOS, Uhr als Parameter. |
| `state_bus` | Ein Zustandsobjekt mit Versionszähler; UI und Webui lesen Schnappschüsse, Befehle laufen über eine Queue in den Regeltask. |
| `cerbo` | MQTT-Client, nur lesen (Kapitel 5). |
| `power` | Betriebszustände, Display an/aus, Light Sleep, Unterspannung (Kapitel 10). |
| `measure` | Messmodus, Logger auf SD, Kalibrier-Assistent (Kapitel 8). |
| `ui` | LVGL-Oberfläche (Kapitel 9). |
| `webui` | Kleine Service-Seite über Ethernet/WLAN: Zustand, Profil herunter- und hochladen, Messdateien, OTA. |
| `safety` | Prüft Sicherheitsregeln und hält den Ausgabezustand (Kapitel 6). |

### 3.3 Tasks

| Task | Priorität | Zyklus | Aufgabe |
| --- | --- | --- | --- |
| `io` | hoch | 20 ms | Eingänge lesen, Ausgänge schreiben, Sicherheitsprüfung |
| `logic` | hoch | 50 ms | Module rechnen, Befehle aus der Queue anwenden |
| `ui` | mittel | LVGL-Takt | Anzeige, Touch |
| `net` | niedrig | Ereignis | MQTT, Webui |
| `measure` | niedrig | Puffer | SD-Schreiben |

Nur der `io`-Task schreibt Ausgänge. Die UI sendet ausschliesslich Befehle an die Queue (wie die Wünsche des Pi in der früheren Architektur).

### 3.4 Host-Build und Simulator

- `host/` baut `signals`, `modules`, `safety`, `profile` und die `ui` mit `hal_sim` und `display_sdl` auf macOS/Linux. Das ersetzt `make dev` aus Version 1.0.
- Simulator-Bedienfeld (Tastatur oder kleines Zusatzfenster): Zündung, Fahrt, DI-Pegel, AI-Werte, Cerbo-Werte.
- Unit-Tests mit Unity oder einem anderen kleinen C-Testframework, laufen ohne Hardware: `make test`.

---

## 4. Treiber

### 4.1 SM-I-009 (Sixteen LV Digital Inputs, Hardware ≥ 3.0)

Quelle: `github.com/SequentMicrosystems/16inpind-rpi`, `src/data.h`. Adresse `(0x20 + Stacklevel) ^ 0x07`, Level 0 = 0x27. Mehrbyte-Werte Little-Endian.

| Register | Länge | Inhalt |
| --- | --- | --- |
| 8 | 2 | Eingänge 1–16 als Bitmaske (Bit 0 = DI1) |
| 10 | 2 | Eingänge mit Wechselspannung |
| 18 / 20 | 2 / 2 | Flankenzählung steigend / fallend aktivieren, Bit je Kanal |
| 22 | 1 | Zähler zurücksetzen (Kanalnummer) |
| 25 | 16 × 4 | Flankenzähler je Kanal |
| 157 / 159 / 160 | 2 / 1 / 2 | Diagnose 3,3 V mV, Temperatur, 5 V mV |
| 202 ff. | | Watchdog (beim Start lesen und deaktivieren, falls aktiv) |
| 222 | 2 | Interrupt bei Änderung, Bit je Kanal |
| 251–254 | 1 je | Hardware- und Firmware-Version |

Beim Start Version lesen und loggen. DI6 (Geschwindigkeit) über den Flankenzähler auswerten, nicht über Abfrage.

### 4.2 SM-H-001 (Home Automation HAT)

Registerkarte aus dem offenen Sequent-Repository für die Home Automation Card portieren (Relais, Analogeingänge, Version). Adresse nach Stack-Jumper, muss sich von 0x27 unterscheiden. Beim Start Platinenversion lesen: Analogeingänge max. 3,0 V, v3 mit eingebautem 15-kΩ-Pull-up (HARDWARE 4.3).

**Wichtig:** Die Karte hat einen eigenen Prozessor und hält die Relais, wenn der P4 neu startet. Deshalb Regel S1 und S11.

### 4.3 Sollwertleiter

H1–H4 über GPIO, stromlos = alle Segmente überbrückt = 1750 Ω. Stufe 0–15 binär (H1 = Bit 0). Widerstand = 1750 + Summe der aktiven Segmente (27,4 / 54,9 / 110 / 237 Ω).

### 4.4 Anzeige und Touch

- Viewe-Komponente `esp_lcd_dsi` mit der Init-Sequenz für das 6,8"-Panel. Nativ 480 × 1280 hochkant; LVGL arbeitet quer mit 1280 × 480. Drehung über den PPA des P4 oder die Rotation des Panels, je nachdem, was die Init-Sequenz anbietet.
- Hintergrundbeleuchtung: PWM laut Viewe-Antwort. Bis zur Klärung `set_brightness` nur an/aus.
- Touch: Controller laut Viewe-Datenblatt, INT als Weckquelle aus Light Sleep.

---

## 5. Victron Cerbo GX (MQTT, nur lesen)

- WLAN-Station am Access Point des Cerbo. SSID, Passwort und Broker-IP im Profil. Reconnect mit Backoff 1–30 s.
- Portal-ID beim Start aus `N/+/system/0/Serial`. VE.Bus-Instanz aus `N/<id>/vebus/+/ProductId`.
- Keepalive: alle 30 s `R/<id>/keepalive` (Payload nach dbus-flashmq-README der installierten Venus-OS-Version; im Code verlinken).
- Gelesen:
  - `system/0/Dc/Battery/{Soc,Voltage,Current,Power}`, `system/0/Dc/Pv/Power`, `system/0/Dc/Charger/Power`
  - `vebus/<inst>/{State,Mode,Ac/ActiveIn/ActiveInput,Ac/ActiveIn/L1/{V,I,F,P},Ac/Out/L1/{V,I,F,P,S},Ac/PowerMeasurementType,Dc/0/Power}`
  - `vebus/<inst>/Alarms/{L1/Overload,LowBattery,HighTemperature,L1/Ripple}`
- Jeder Wert mit Empfangszeit. Alter > 10 s → Anzeige grau mit Alter. Fehlender Wert → leer, nie ein Ersatzwert.
- **Nie** auf `W/…` publizieren.
- Stromsparen: Modem Sleep des C6 zulassen; Keepalive-Intervall danach ausrichten.

---

## 6. Sicherheitsregeln (mit Tests, vor jeder Hardware-Freigabe)

| Nr. | Regel |
| --- | --- |
| S1 | Erste Aktion nach jedem Start, vor UI und Netzwerk: K1–K4 aus (SM-H-001), H1–H4 stromlos. Innert 500 ms nach Reset. |
| S2 | K1 und K2 nie gleichzeitig. Richtungswechsel nur nach 300 ms mit beiden aus. |
| S3 | Dachrelais fällt ab, wenn 300 ms keine Halten-Meldung vom Touch kommt (Finger weg, Touch-Fehler, UI-Task hängt) oder 60 s Laufzeit pro Druck erreicht sind (Profil). |
| S4 | Öffnen des Dachs gesperrt, solange das Fahrzeug fährt (Flankenzähler DI6, Schwelle im Profil). Schliessen bleibt erlaubt. Läuft Öffnen beim Anfahren: sofort stoppen. |
| S5 | Fällt der SM-I-009 aus (3 I²C-Fehler in Folge): DI-Werte `stale`, Dach gesperrt, Meldung. |
| S6 | Kühlbox: Mindestlaufzeit 2 min, Mindestpause 3 min (Profil). |
| S7 | Heizungsstufe: H-Relais einzeln umschalten, 50 ms Abstand. |
| S8 | Display-Aus oder Light Sleep nie, solange das Dach fährt. |
| S9 | Ausfall des Cerbo ändert nichts an Dach, Kühlbox und Heizung. |
| S10 | Task-Watchdog von ESP-IDF auf `io` und `logic` (1 s). Hängt ein Task: Neustart, danach greift S1. |
| S11 | Fällt der SM-H-001 aus (3 I²C-Fehler in Folge): Dach gesperrt, Meldung, weiter versuchen, K1/K2 auszuschalten. Offener Punkt: Bietet die Karte eine Relais-Abschaltung bei ausbleibender Kommunikation? Falls ja, mit 1 s einschalten. |

Jede Regel hat mindestens einen Host-Test in `tests/safety/`, benannt nach der Nummer (`test_s3_roof_hold_timeout.c`).

---

## 7. Funktionsmodule

Inhalt wie Version 1.0, Kapitel 7. Kurzfassung:

### 7.1 Dach (`roof`)
- Eingänge DI1 (T32b/15), DI2 (T32b/14), Fahrt aus DI6. Ausgänge K1 (T32b/11), K2 (T32b/12).
- Wahrheitstabelle im Profil (`roof.truth_table`, `TODO_MEASURE`). Ohne Tabelle: Stellung `unknown`, Bedienung trotzdem möglich (Hold-to-run).
- Zustände `closed`, `open`, `moving`, `between`, `unknown`, `fault`. Warnung `roof_open_ignition`.
- **Abnahme:** S2, S3, S4 grün. Im Simulator: Taste halten → K1 an; loslassen → K1 innert 300 ms aus.

### 7.2 Kühlbox (`fridge`)
- Thermostat auf G817 (AI1): an bei `temp ≥ target + 1,5`, aus bei `temp ≤ target − 1,0` (Profil). Soll 1–10 °C.
- Diagnose DI3 (T32b/9): low-aktive Pulse 0,25 s, Sequenz alle 4 s, Code erst nach zwei gleichen Sequenzen. Codes 1–6 nach Secop.
- G817 `open`/`short`/`uncalibrated` → Thermostat aus, Meldung.
- Anzeige «Kühlung angefordert», nicht «Kompressor läuft».
- **Abnahme:** S6; Pulsdecoder-Test mit aufgezeichneten Folgen für Codes 1–6 und Störpulsen.

### 7.3 Standheizung (`heater`)
- K4 = S+ (T6ap/3), DI4 = Betriebsanzeige (T6ap/2); Zuordnung noch messen.
- Phasen `off` → `start` → (DI4 an) `heating` → (aus) `afterrun` → (DI4 aus) `off`. `start` ohne DI4 nach 120 s → `fault`.
- Anzeige in Stufen, bis `heater.level_to_c` kalibriert ist.
- **Abnahme:** S7; Phasenwechsel im Simulator inkl. Nachlauf.

### 7.4 Tanks, 7.5 Temperaturen, 7.6 Fahrzeug, 7.7 Energie
Wie Version 1.0. Änderung 7.6: Geschwindigkeit über den Flankenzähler des SM-I-009, Fenster 1 s, «fährt» ab `speed.moving_min_edges`.

---

## 8. Messmodus (`measure`)

Wie Version 1.0, Kapitel 8, mit diesen Änderungen:
- Ablage auf microSD (`/sdcard/measure/<datum>.csv`), Profil-Änderungen in LittleFS mit Backup.
- DI-Flanken mit Zeitstempel aus dem Flankenzähler und der Abfrage (20 ms); 1-ms-Auflösung nur, falls der Interrupt des SM-I-009 genutzt wird.
- Auswertung `tools/plot_measure.py` auf dem Mac. Dateien über die Webui herunterladen.
- Relais gesperrt, solange `measure.outputs_locked` gesetzt ist.

---

## 9. Oberfläche (LVGL)

Vorlage: `ui/reference/Camping_Unit_Oberflaeche_v2.html`. Layout, Farben, Navigation und Texte von dort übernehmen und in LVGL nachbauen. Keine Webtechnik auf dem Gerät.

- Leinwand 1280 × 480 quer: Kopfzeile 64 px, Inhalt 416 px.
- Navigation: Übersicht (6 Kacheln) → Detailseite → «‹ Übersicht». Zahnrad → Einstellungen → Expertenansicht mit Reitern Signale / Stecker T32b / MultiPlus / Verbindung / Messmodus.
- Rückkehr zur Übersicht nach 1/2/5 min ohne Bedienung (Standard 2 min), nie während das Dach fährt.
- Tastengrössen: Plus/Minus 112 px, Ein/Aus 88 px hoch, Dachtasten volle Höhe, sonst mindestens 52 px.
- Eine Akzentfarbe; Grün/Gelb/Rot nur für Zustände. Tag- und Nachtschema, Automatik über 58d.
- Texte aus `ui/i18n/de.json`, beim Build in eine C-Tabelle übersetzt. Keine Texte im C-Code.
- Schrift: mit `lv_font_conv` erzeugen, inkl. ä ö ü Ä Ö Ü ° – ‹ und Ziffern mit fester Breite.
- Dachtasten senden «halten» im 100-ms-Takt, solange der Finger liegt (Grundlage für S3).
- Erste Berührung nach dem Aufwecken: Summer, Display an, keine Bedienung.
- Helligkeit: Regler wirkt, sobald PWM geklärt ist; vorher nur an/aus.

**Abnahme:** Jede Seite des Prototyps läuft im Host-Simulator. Zustandswechsel erscheinen innert 200 ms. Auf dem Gerät mindestens 30 Bilder/s beim Seitenwechsel.

---

## 10. Betriebszustände und Stromsparen

Die Firmware läuft immer. Kein Herunterfahren, keine Wake-MCU.

| Zustand | Eintritt | Display | Austritt |
| --- | --- | --- | --- |
| `drive` | 15a an | an, Nachtmodus über 58d | 15a aus → `active` |
| `active` | Berührung, 15a aus | an | 60 s ohne Bedienung (Einstellung) → `idle` |
| `idle` | – | aus, Hintergrundbeleuchtung aus | Berührung oder 15a an |

- In `idle` Light Sleep zwischen den Zyklen, soweit der Regelzyklus es zulässt; Modem Sleep des C6.
- Unterspannung (Cerbo-Spannung, sonst AI8 < Schwelle im Profil, 60 s): Kühlanforderung aus, Meldung, Display bleibt aus. Heizungsnachlauf wird nie unterbrochen.
- Ruhestrom in `idle` messen und dokumentieren (Hardware O19).

---

## 11. Repository

```
camping-unit/
  CLAUDE.md
  README.md
  docs/            HARDWARE.md, SOFTWARE_SPEC.md, E153_Pin_Check_Nr133.md
  profiles/        t6-california-a8l-2018.json (+ JSON-Schema)
  firmware/        ESP-IDF-Projekt: main/, components/, sdkconfig.defaults, partitions.csv
  host/            Simulator und Tests (CMake), hal_sim, display_sdl
  tests/           unit/, safety/
  ui/              reference/Camping_Unit_Oberflaeche_v2.html, i18n/de.json, fonts/
  tools/           plot_measure.py, i18n_gen.py
```

`make test` (Host), `make sim` (Simulator), `make build` / `make flash` (Gerät).

---

## 12. Meilensteine

| M | Inhalt | Fertig, wenn |
| --- | --- | --- |
| M1 | Repo-Gerüst, Profil und Schema, `signals`, `modules`, `safety` mit Host-Tests, Simulator mit LVGL über SDL, Übersicht und eine Detailseite | `make test` grün, `make sim` zeigt Übersicht und Dach-Detail, S1–S4 als Tests grün |
| M2 | Ganze Oberfläche im Simulator, i18n, Schrift, Tag/Nacht | alle Seiten des Prototyps im Simulator bedienbar |
| M3 | Gerät: Board bringup, Display (Viewe-Komponente), Touch, Hintergrundbeleuchtung | Oberfläche auf dem Gerät, Touch geht, Startzeit gemessen |
| M4 | Treiber SM-I-009, SM-H-001, Sollwertleiter auf dem Tisch; Cerbo-MQTT über WLAN | Rohwerte in der Expertenansicht, Victron-Werte vom echten Cerbo |
| M5 | Messmodus, SD-Logger, Kalibrier-Assistent, Webui mit Dateidownload | Messfahrt mit Original-E153 möglich |
| M6 | (Daniel) Messungen am Fahrzeug, Profil füllen | keine `TODO_MEASURE` in Pflichtfeldern |
| M7 | Betriebszustände, Light Sleep, Unterspannung | `idle`-Ruhestrom gemessen und dokumentiert |
| M8 | Schaltfunktionen Heizung, Kühlbox, Dach; alle S-Tests grün | jede Funktion einzeln am Fahrzeug freigegeben |
| M9 | OTA, Ereignislog, Feinschliff | Einbau-Version 2.0 |

M1 und M2 laufen ohne Hardware und können sofort beginnen.

---

## 13. Offene Punkte

| Nr. | Frage | Standard bis zur Klärung |
| --- | --- | --- |
| O2 | Regelt J1029 selbst? | Thermostat in der Firmware |
| O5 | Ist A11 noch eingebaut? | DI5 nur loggen |
| O6 | Wahrheitstabelle Dach, Pegel aller Ein-/Ausgänge | aus Messmodus |
| O7 | Kennlinien G817, G249, G120, G126, Heizungs-Sollwert | `TODO_MEASURE`, «nicht kalibriert» |
| O8 | D+ an T32b/29? | nur loggen |
| O11 | Unterspannungsschwelle | 11,8 V für 60 s |
| O15 | Viewe: MIPI-Version, 2 Lanes, PWM, Touch-INT, Init-Sequenz | `display_t`-Schnittstelle, Rückfall `display_nhd68` |
| O16 | Belegung DSI-Stecker und 5-V-Einspeisung des Boards | Viewe-Schaltplan |
| O17 | SM-H-001: Adresse, eigene Pull-ups, 3,3 V vom Header, Relais-Abschaltung ohne Kommunikation (S11) | Sequent-Repository und Schaltplan |
| O19 | Ruhestrom P4 mit WLAN | messen in M7 |
| O20 | SM-H-001 direkt auf den Header des Boards stecken? | abgesetzt, bis geklärt |
