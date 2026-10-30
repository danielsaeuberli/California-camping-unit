# Cali Camping Unit – Software-Spezifikation

Version 1.0 | 30. Oktober 2026 | Fahrzeug: VW T6 California 2.0 TDI 4Motion, MJ 2018, Campingausrüstung A8L/A8R

Ersetzt Software-Plan v0.3. Neu gegenüber v0.3: schlankes Betriebssystem ohne Chromium und ohne lokalen MQTT-Broker (Kapitel 2 und 3), Oberfläche nach Prototyp v2 (Kapitel 9), Abnahmekriterien pro Modul.

Hardware: `docs/HARDWARE.md`. Pinbelegung und Kalibrierwerte: `profiles/t6-california-a8l-2018.yaml`.

---

## 1. Umfang

Die Unit ersetzt das Bedienteil E153. Die Original-Steuergeräte bleiben im Fahrzeug und werden nur angesteuert:

| Gerät | VW-Bezeichnung | Was die Unit tut |
| --- | --- | --- |
| Dachhydraulik | J768 | Fahranforderung AUF/ZU (K1/K2), Rückmeldungen lesen |
| Kühlbox | J698 / J1029, Secop BD35F | Kühlanforderung (K3), Thermostat auf G817, Fehlercodes lesen |
| Standheizung | J604, Eberspächer Airtronic | Ein/Aus über S+ (K4), Sollwert über Widerstandsleiter (H1–H4), Betriebsanzeige lesen |
| Tanks | G120 Frisch, G126 Abwasser | Stufen anzeigen |
| Temperaturfühler | G817 Kühlbox, G249 Aussen | anzeigen |
| Victron | Cerbo GX, MultiPlus Compact 12/1600 | Batterie, Laden, 230 V anzeigen (nur lesen) |
| Fahrzeug | 15a, 86s, D+, 58d, Geschwindigkeit | Power-Zustände, Fahrsperre, Nachtmodus |

**Nicht im Umfang v1:** CAN/LIN zum Fahrzeug, Zugriff aus dem Internet, Schreiben auf den Cerbo, Licht-Dimmer, Batterie-Management.

---

## 2. Plattform und Betriebssystem (schlank)

### 2.1 Ziele

| Grösse | Ziel |
| --- | --- |
| Kaltstart bis bedienbare Oberfläche | ≤ 15 s (messen in M1) |
| Laufende Dienste ausser Kernel/systemd | `camperd`, `cog`, `systemd-journald`, `systemd-networkd`, `sshd` (nur Service) |
| RAM im Betrieb | ≤ 600 MB |
| Image auf der SD-Karte | ≤ 2 GB belegt |
| Schreibzugriffe im Betrieb | nur Datenpartition `/data` |

### 2.2 Basis

Raspberry Pi OS Lite 64 bit (Bookworm). Kein Desktop, kein X11, kein Wayland-Compositor, kein Chromium.

Begründung gegen ein eigenes Buildroot-/Yocto-Image: deutlich mehr Aufwand für Bau und Pflege. Buildroot bleibt Option, falls das Startzeitziel mit Pi OS Lite nicht erreicht wird (O13).

### 2.3 Pakete

Zusätzlich zu Pi OS Lite nur:

```
cog                      # WPE-WebKit-Browser, Anzeige der Oberfläche
python3-venv
libgpiod2
```

Python-Pakete im venv unter `/opt/camperd/venv`: `aiohttp`, `aiomqtt`, `smbus2`, `gpiod`, `pyserial-asyncio`, `pyyaml`. Mit Hash-Pinning (`requirements.lock`).

### 2.4 Entfernen bzw. deaktivieren

```
bluetooth hciuart triggerhappy avahi-daemon ModemManager
cups* rsyslog dphys-swapfile man-db apt-daily* apt-daily-upgrade*
NetworkManager (durch systemd-networkd ersetzt) wpa_supplicant (nur im Servicemodus starten)
keyboard-setup console-setup (nach Ersteinrichtung)
```

### 2.5 `config.txt` und Kernel-Parameter

```
# /boot/firmware/config.txt (Auszug)
dtparam=i2c_arm=on
dtparam=audio=off
camera_auto_detect=0
display_auto_detect=0
disable_splash=1
boot_delay=0
dtoverlay=disable-bt
dtoverlay=uart2-pi5        # Wake-MCU an GPIO4/5 (HARDWARE.md, O4)
hdmi_force_hotplug=1
# HDMI-Modus für Viewe 480x1280 nach Datenblatt der Treiberplatine; Drehung auf 1280x480 quer
```

`cmdline.txt`: `quiet loglevel=3 logo.nologo consoleblank=0 fsck.repair=yes`.

### 2.6 Dateisysteme

- Root schreibgeschützt über `overlayroot` (raspi-config → Overlay File System).
- Eigene Partition `/data` (ext4, `noatime`, `commit=60`): Profil-Kopie mit Kalibrierwerten, Einstellungen, Ereignislog, Messdaten.
- `journald`: `Storage=volatile`, `RuntimeMaxUse=16M`. Wichtige Ereignisse schreibt `camperd` selbst nach `/data/log/events.jsonl` (rotierend, max. 10 MB).
- Kein Swap.

### 2.7 Netzwerk

- `eth0` → Cerbo GX, statisch (`systemd-networkd`), z. B. Pi `192.168.77.2/24`, Cerbo `192.168.77.1` (Adresse am Cerbo fest einstellen).
- WLAN aus. Servicemodus (Einstellungen → Expertenansicht) startet `wpa_supplicant` für Updates und SSH; schaltet nach 30 min wieder ab.
- `sshd` nur mit Schlüssel, nur im Servicemodus oder über Ethernet.

### 2.8 Oberfläche anzeigen

```
cog --platform=drm http://127.0.0.1:8080/
```

Als systemd-Dienst `camper-ui.service`, `After=camperd.service`, `Restart=always`. Touch kommt über libinput an `cog`.

**Rückfallebene,** falls `cog` auf DRM mit Pi 5 + Viewe nicht sauber läuft (Prüfung in M1): `cage -- cog --platform=wl …`. Chromium nur als letzte Option, und dann mit begründetem Entscheid.

### 2.9 Updates

Kein Git und kein Paketmanager im Betrieb. `make deploy` vom Mac:

1. Overlay für einen Neustart abschalten (`raspi-config nonint do_overlayfs 1`) oder `/opt/camperd` auf `/data` legen und dort versioniert ablegen (`/data/releases/<version>`, Symlink `current`). **Bevorzugt die zweite Variante,** weil sie ohne Neustart und mit Rückfall auf die vorherige Version auskommt.
2. `rsync` nach `/data/releases/<version>`, Symlink umstellen, `systemctl restart camperd camper-ui`.

### 2.10 Image-Skript

`deploy/build-image.sh` erzeugt aus einem frischen Pi OS Lite reproduzierbar das Zielsystem (Pakete, Abschaltungen, config.txt, Dienste, Partition `/data`, Overlay an). Idempotent, als Bash mit `set -euo pipefail`.

---

## 3. Architektur

### 3.1 Ein Prozess

```
Fahrzeug ─ T32b/T6ap/T2p ─ Breakout X1–X4 ─ SM-H-001 / SM-I-009 / H1–H4
                                               │ I²C, GPIO
Cerbo GX ─ Ethernet, MQTT ──────────┐          │
Wake-MCU ─ UART ───────────────┐    │          │
                               ▼    ▼          ▼
                    camperd (Python, asyncio, ein Prozess)
                      hal → signals → modules → state
                      api: HTTP (statische UI) + WebSocket
                               │ 127.0.0.1:8080
                               ▼
                    cog (WPE WebKit, DRM) → Viewe 1280×480
```

### 3.2 Pakete in `camperd`

| Paket | Aufgabe |
| --- | --- |
| `hal/` | Treiber pro Quelle: `sim`, `sm_h001`, `sm_i009`, `gpio_ladder`, `cerbo_mqtt`, `wake_uart`. Einheitliche Schnittstelle: `read_di()`, `read_ai()`, `write_relay(ch, on)`, `write_gpio(ch, on)`, Ereignisse als `asyncio.Queue`. |
| `profile/` | Lädt und validiert das YAML-Profil. Unbekannte Schlüssel → Fehler. `TODO_MEASURE` → Wert gilt als «nicht kalibriert». |
| `signals/` | Rohwert → Wert mit Zeitstempel und Qualität: `ok`, `stale`, `open`, `short`, `implausible`, `uncalibrated`. Entprellen, Median, Pulszähler, Frequenz. |
| `modules/` | Zustandsmaschinen: `roof`, `fridge`, `heater`, `tanks`, `climate`, `vehicle`, `energy`. Rein logisch, keine I/O, Uhr injiziert. |
| `power/` | Betriebszustände, Shutdown-Bedingungen, Wake-MCU-Protokoll. |
| `api/` | `aiohttp`: liefert `ui/` aus, WebSocket `/ws`, Ereignislog. |
| `measure/` | Messmodus und Kalibrier-Assistent (Kapitel 8). |

Zyklus: Eingänge 20 ms (DI) bzw. 100 ms (AI) lesen, Module rechnen, Ausgänge schreiben, Zustand an die Oberfläche bei Änderung, spätestens jede Sekunde.

---

## 4. Schnittstelle Oberfläche ↔ `camperd` (WebSocket `/ws`)

JSON-Nachrichten, ein Objekt pro Frame. Version im Feld `v`.

**Von `camperd`:**

```json
{"v":1,"type":"state","ts":1761835200123,"data":{
  "power":{"mode":"drive|afterrun|standby|shutdown","ignition":true},
  "roof":{"pos":"closed|open|moving|between|unknown|fault","cmd":"none|open|close","block":"none|moving|unit_io|timeout"},
  "fridge":{"on":true,"target_c":5,"temp_c":4.8,"temp_q":"ok","request":false,"fault":null},
  "heater":{"on":false,"level":8,"phase":"off|start|heating|afterrun|fault","afterrun_s":0},
  "tanks":{"fresh_step":2,"fresh_q":"ok","waste_full":false,"waste_q":"ok"},
  "outside_c":12.5,
  "vehicle":{"moving":false,"d_plus":null,"t86s":true,"dim58":false},
  "alerts":[{"id":"waste_full","level":"warn"}]
}}
{"v":1,"type":"victron","ts":…,"data":{"soc":{"val":76,"age_s":1.2}, "battery_v":{…}, "battery_a":{…}, "pv_w":{…}, "charger_w":{…}, "vebus":{"state":9,"mode":3,"ac_in":{…},"ac_out":{…},"alarms":{…}}}}
{"v":1,"type":"io","ts":…,"data":{"di":[0,1,…16 Werte],"k":[0,0,0,0],"h":[0,0,0,1],"ai_v":[2.21,1.80,0.60,1.43,null,null,null,null],"ladder_ohm":1987.0}}
{"v":1,"type":"ack","id":"c-123","ok":true}
{"v":1,"type":"ack","id":"c-124","ok":false,"reason":"blocked_moving"}
```

`io` nur senden, wenn die Expertenansicht offen ist (`subscribe`-Nachricht).

**Von der Oberfläche:**

```json
{"v":1,"type":"cmd","id":"c-123","target":"fridge","set":{"on":true,"target_c":5}}
{"v":1,"type":"cmd","id":"c-124","target":"heater","set":{"on":true,"level":8}}
{"v":1,"type":"hold","target":"roof","dir":"open","seq":412}
{"v":1,"type":"subscribe","topics":["io"]}
{"v":1,"type":"settings","set":{"brightness":80,"theme":"auto","idle_min":2}}
```

- `hold` alle 100 ms, solange die Dachtaste gedrückt ist. `seq` steigt monoton.
- Verbindungsabbruch = Last Will: Die Oberfläche sperrt alle Bedienelemente und zeigt «Steuerung offline». `camperd` behandelt einen abgerissenen Socket wie ausbleibende `hold`-Nachrichten.

---

## 5. Victron Cerbo GX (MQTT, nur lesen)

- Verbindung `192.168.77.1:1883`, Client-ID `camperd`. Reconnect mit Backoff 1–30 s.
- Portal-ID beim Start aus `N/+/system/0/Serial` ermitteln. VE.Bus-Instanz aus `N/<id>/vebus/+/ProductId`.
- Keepalive: alle 30 s `R/<id>/keepalive` (Payload nach dbus-flashmq-README der installierten Venus-OS-Version; beim Aufsetzen nachlesen und im Code verlinken).
- Gelesen:
  - `system/0/Dc/Battery/{Soc,Voltage,Current,Power}`, `system/0/Dc/Pv/Power`, `system/0/Dc/Charger/Power`
  - `vebus/<inst>/{State,Mode,Ac/ActiveIn/ActiveInput,Ac/ActiveIn/L1/{V,I,F,P},Ac/Out/L1/{V,I,F,P,S},Ac/PowerMeasurementType,Dc/0/Power}`
  - `vebus/<inst>/Alarms/{L1/Overload,LowBattery,HighTemperature,L1/Ripple}`
- Jeder Wert mit Empfangszeit. Alter > 10 s → Oberfläche zeigt grau mit Alter. Fehlender Wert → `null`, nie ein Ersatzwert.
- **Nie** auf `W/…` publizieren.

---

## 6. Sicherheitsregeln (mit Tests, vor jeder Hardware-Freigabe)

| Nr. | Regel |
| --- | --- |
| S1 | Beim Start sind K1–K4 aus und H1–H4 stromlos (NC geschlossen = 1750 Ω = tiefste Heizstufe), bevor `camperd` irgendeine Nachricht annimmt. |
| S2 | K1 und K2 nie gleichzeitig. Richtungswechsel nur nach 300 ms mit beiden aus. |
| S3 | Dachrelais fällt ab, wenn 300 ms kein `hold` kommt, der WebSocket abreisst oder 60 s Laufzeit pro Druck erreicht sind (Wert im Profil). |
| S4 | Öffnen des Dachs gesperrt, solange Geschwindigkeitsimpulse anliegen (Profil: `speed.moving_hz`). Schliessen bleibt erlaubt. Läuft ein Öffnen-Befehl, wenn die Fahrt beginnt: sofort stoppen. |
| S5 | Fällt der SM-I-009 aus (3 I²C-Fehler in Folge): DI-Werte `stale`, Dach gesperrt, Meldung. |
| S6 | Kühlbox: Mindestlaufzeit 2 min, Mindestpause 3 min (Profil). |
| S7 | Heizungsstufe: H-Relais nur einzeln umschalten, 50 ms Abstand. Nie zwei gleichzeitig. |
| S8 | Kein Shutdown, solange die Heizung im Nachlauf ist oder das Dach fährt. |
| S9 | Ausfall des Cerbo ändert nichts an Dach, Kühlbox und Heizung. |
| S10 | Ein Watchdog im `camperd` (asyncio-Task) prüft den Hauptzyklus. Hängt er > 2 s: alle Relais aus, Prozess beenden, systemd startet neu. |

Jede Regel hat mindestens einen Test in `tests/safety/`, benannt nach der Nummer (`test_s3_roof_hold_timeout.py`).

---

## 7. Funktionsmodule

### 7.1 Dach (`roof`)

- Eingänge: DI1 (T32b/15), DI2 (T32b/14), Geschwindigkeit DI6. Ausgänge: K1 (T32b/11), K2 (T32b/12).
- Welche Richtung K1/K2 bedeutet und wie DI1/DI2 die Stellung kodieren, kommt aus der Wahrheitstabelle im Profil (`roof.truth_table`, `TODO_MEASURE`). Ohne Tabelle: Stellung `unknown`, Bedienung trotzdem möglich (Hold-to-run).
- Zustände: `closed`, `open`, `moving`, `between`, `unknown`, `fault`.
- Warnung `roof_open_ignition`, wenn Zündung an und Dach nicht `closed`.

**Abnahme:** S2, S3, S4 als Tests grün. Im Simulator: Taste halten → K1 an; Taste loslassen → K1 innert 300 ms aus; WebSocket trennen während des Haltens → K1 innert 300 ms aus.

### 7.2 Kühlbox (`fridge`)

- Thermostat auf G817 (AI1): Anforderung an bei `temp ≥ target + 1.5`, aus bei `temp ≤ target − 1.0` (Profil). Soll 1–10 °C.
- Diagnose an DI3 (T32b/9): low-aktive Pulse 0,25 s, Sequenz alle 4 s. Code erst nach zwei gleichen Sequenzen melden. Codes 1–6 nach Secop (Text in `i18n`).
- G817 `open`/`short`/`uncalibrated` → Thermostat aus, Meldung. Option `fallback_duty` im Profil (Standard aus).
- Oberfläche sagt «Kühlung angefordert», nicht «Kompressor läuft».

**Abnahme:** S6 als Test; Pulsdecoder-Test mit aufgezeichneten Pulsfolgen für Codes 1–6 und mit Störpulsen.

### 7.3 Standheizung (`heater`)

- K4 = S+ (T6ap/3, bestätigt). DI4 = Betriebsanzeige (T6ap/2).
- Stufe 0–15 → H1–H4 binär (H1 = Bit 0 … H4 = Bit 3). Relais an = NC offen = Segment wirksam. Widerstand = 1750 + Σ Segmente (27,4 / 54,9 / 110 / 237 Ω).
- Phasen: `off` → (ein) `start` → (DI4 an) `heating` → (aus) `afterrun` → (DI4 aus) `off`. `start` ohne DI4 nach 120 s → `fault`.
- Anzeige in Stufen, bis `heater.level_to_c` im Profil kalibriert ist.

**Abnahme:** S7, S8 als Test; Phasenwechsel im Simulator inkl. Nachlauf-Restzeit.

### 7.4 Tanks (`tanks`)

- AI2 (G120) → Widerstand → Stufe 0–3 über Schwellen im Profil. Median 5 s, bei Fahrt 30 s. Neue Stufe erst nach 10 s stabil.
- AI3 (G126) → voll / nicht voll.
- Nie Prozent anzeigen.

### 7.5 Temperaturen (`climate`)

- G817 und G249 als NTC, Steinhart-Hart aus drei Kalibrierpunkten im Profil. Fehlen sie: `uncalibrated`.
- Aussentemperatur-Quelle im Profil: `g249` (Standard) oder `cerbo`.

### 7.6 Fahrzeug (`vehicle`)

15a (DI10), 86s (DI8), D+ (DI9, Hypothese: nur loggen, bis bestätigt), 58d (DI7, Nachtmodus), Landstrom A11 (DI5, nur loggen, bis geklärt ist, ob A11 noch eingebaut ist).

Geschwindigkeit (DI6): nur «fährt / steht», nicht km/h. Flanken in einem Fenster von 1 s zählen; ab `speed.moving_min_edges` gilt «fährt». Ob der SM-I-009 bei 20-ms-Abfrage genug Flanken sieht, ist offen (O14).

### 7.7 Energie (`energy`)

Bereitet die Cerbo-Werte für die Oberfläche auf (Kapitel 5), inkl. Nettoleistung, Ladephase aus `vebus/State`, Auslastung = `Ac/Out/L1/S` / 1600 VA.

---

## 8. Messmodus (`measure`)

Wird vor den Schaltfunktionen gebraucht (M3). Original-E153 bleibt angeschlossen, die Unit hört über einen Y-Abgriff mit. Relais bleiben gesperrt (Profil-Schalter `measure.outputs_locked: true`).

1. **Logger:** alle DI mit 1 ms Zeitstempel (Flanken), AI mit 10 Hz nach `/data/measure/<datum>.csv`. Marker per Tastendruck in der Oberfläche.
2. **Wahrheitstabelle Dach:** geführter Ablauf («Jetzt am Original öffnen …»), schreibt `roof.truth_table` ins Profil.
3. **Pulsdecoder Kühlbox** live.
4. **Kalibrierung:** Tankstufen, NTC-Punkte, Heizungs-Sollwert. Schreibt nach `/data/profile.yaml` mit Backup.
   Pegel der Dachbefehle T32b/11 und /12 über AI6/AI7 (Spannungsteiler) aufzeichnen.
   Im Messmodus keine Pull-ups an AI1–AI4 (HARDWARE.md 4.3).
5. **Signalprüfung:** 86s, D+ (Motor an/aus), Landstrom.

`tools/plot_measure.py` macht aus den CSV Diagramme (matplotlib nur auf dem Mac, nicht auf dem Pi).

---

## 9. Oberfläche

Vorlage: `ui/reference/Camping_Unit_Oberflaeche_v2.html`. Layout, Farben, Navigation und Texte von dort übernehmen. Die Demo-Logik und die Demo-Steuerung unter dem Bildschirm entfallen und werden durch den WebSocket ersetzt.

- Feste Leinwand 1280 × 480: Kopfzeile 64 px, Inhalt 416 px.
- **Navigation:** Übersicht (6 Kacheln) → Detailseite → «‹ Übersicht». Zahnrad → Einstellungen → «Expertenansicht» → Reiter Signale / Stecker T32b / MultiPlus / Verbindung / Messmodus. Keine weiteren Menüpunkte.
- **Rückkehr zur Übersicht** nach 1/2/5 min ohne Bedienung (Einstellung, Standard 2 min), nie während das Dach fährt.
- **Tastengrössen:** Plus/Minus 112 px, Ein/Aus 88 px hoch, Dachtasten volle Höhe, sonst mindestens 52 px.
- **Farben:** eine Akzentfarbe; Grün/Gelb/Rot nur für Zustände. Tag- und Nachtschema, Automatik über 58d.
- **Texte** aus `ui/i18n/de.json`. Keine Texte im JS.
- **Keine externen Ressourcen** (keine CDNs, keine Webfonts). Systemschrift.
- **Erster Touch nach dem Aufwecken** wird verworfen (kommt als Flag von `camperd`).
- **Helligkeit:** Regler schickt `settings.brightness`. `camperd` setzt die Hintergrundbeleuchtung, sobald geklärt ist, wie die Viewe-Treiberplatine das erlaubt (O10).

**Abnahme:** Jede Seite des Prototyps funktioniert mit `make dev` gegen den Simulator. Zustandswechsel im Simulator erscheinen innert 200 ms in der Oberfläche.

---

## 10. Power-Management und Wake-MCU

### 10.1 Zustände

| Zustand | Eintritt | Display | Austritt |
| --- | --- | --- | --- |
| `drive` | 15a an | an | 15a aus → `afterrun` |
| `afterrun` | 15a aus | an | 60 s ohne Bedienung → `standby` |
| `standby` | – | aus | Touch → `afterrun`; 30 min ohne Bedienung und Shutdown-Bedingungen erfüllt → `shutdown` |
| `shutdown` | – | aus | `SHUTDOWN` an MCU, `systemctl poweroff` |

Shutdown-Bedingungen: Kühlbox-Thermostat aus, Heizung `off`, Dach steht. Unterspannung (Cerbo-Spannung < Schwelle im Profil, 60 s) erzwingt Shutdown ausser bei Heizungsnachlauf. Ohne Cerbo-Spannung kein automatischer Unterspannungs-Shutdown, nur eine Warnung.

### 10.2 Protokoll Pi ↔ MCU

UART 115200 8N1, ASCII-Zeilen mit `\n`:

| Pi → MCU | MCU → Pi |
| --- | --- |
| `HB` (alle 5 s) | `WAKE ignition` / `WAKE touch` / `WAKE watchdog` (auf `WAKE?`) |
| `WAKE?` | `IGN 1` / `IGN 0` bei Änderung |
| `SHUTDOWN` | `OK SHUTDOWN` |
| `VER?` | `VER 1.0.0` |

### 10.3 MCU-Firmware (`wake-mcu/`)

- Zündung an → DCDC-Enable an.
- Touch-IRQ bei aus → DCDC-Enable an, Grund `touch`.
- Nach `SHUTDOWN`: warten, bis Power-Good fällt, +5 s, dann DCDC-Enable aus.
- Kein `HB` für 10 min bei Zündung aus → DCDC aus/an (Neustart). Bei Zündung an nie hart abschalten.
- Ruhestrom im Aus-Zustand messen und dokumentieren (Ziel < 5 mA an 12 V).

---

## 11. Meilensteine

| M | Inhalt | Fertig, wenn |
| --- | --- | --- |
| M1 | Repo-Gerüst, Profil-Loader, `sim`-HAL, Modulgerüste, WebSocket-API, Oberfläche aus Prototyp an den Socket gebunden. **Plattform-Prüfung auf dem Pi:** Image-Skript, `cog --platform=drm` mit Viewe + Touch, Kaltstartzeit messen | `make dev` läuft auf dem Mac; auf dem Pi zeigt `cog` die Oberfläche, Touch geht, Startzeit gemessen und notiert |
| M2 | Treiber `sm_h001`, `sm_i009`, `gpio_ladder`, `cerbo_mqtt` auf dem Tisch | Rohwerte in der Expertenansicht, Victron-Werte vom echten Cerbo |
| M3 | Messmodus und Kalibrier-Assistent | Messfahrt mit Original-E153 möglich, CSV und Profil-Updates werden geschrieben |
| M4 | (Daniel) Messungen am Fahrzeug, Profil füllen | keine `TODO_MEASURE` mehr in den Pflichtfeldern |
| M5 | Lesende Module: Tanks, Temperaturen, Energie, Fahrzeug | Unit läuft im Auto parallel zum Original, ohne zu schalten |
| M6 | Wake-MCU-Firmware und Power-Management | Wake, Nachlauf, Standby, Shutdown am Tisch geprüft, Ruhestrom gemessen |
| M7 | Schaltfunktionen in der Reihenfolge Heizung, Kühlbox, Dach; alle S-Tests grün | jede Funktion einzeln am Fahrzeug freigegeben |
| M8 | Feinschliff: Meldungen, Nachtmodus, Ereignislog, Updates über `/data/releases` | Einbau-Version 1.0 |

---

## 12. Offene Punkte (mit Standard, bis geklärt)

| Nr. | Frage | Standard bis zur Klärung |
| --- | --- | --- |
| O1 | Läuft `cog` (DRM) auf Pi 5 mit Viewe und Touch? | M1 prüfen; Rückfall `cage` + `cog -P wl` |
| O2 | Regelt J1029 selbst? | Thermostat im Pi |
| O3 | Was tut der Watchdog des SM-H-001, wenn der Pi über den DCDC-USB gespeist wird? | HAT-Watchdog aus; Software-Watchdog S10 |
| O4 | Belegt der SM-H-001 UART oder weitere GPIO? | vor M2 im Datenblatt prüfen; GPIO-Belegung in HARDWARE.md anpassen |
| O5 | Ist A11 noch eingebaut? | DI5 nur loggen |
| O6 | Wahrheitstabelle Dach, Pegel aller Ein-/Ausgänge | aus Messmodus (M3/M4) |
| O7 | Kennlinien G817, G249, G120, G126, Heizungs-Sollwert | `TODO_MEASURE`, Anzeige «nicht kalibriert» |
| O8 | D+ an T32b/29? | nur loggen |
| O9 | Touch-Controller mit IRQ für Wake? | ohne IRQ: Wake nur über Zündung |
| O10 | Helligkeitssteuerung der Viewe-Treiberplatine | Regler ohne Wirkung, Hinweis in der Oberfläche |
| O11 | Unterspannungsschwelle | 11,8 V für 60 s |
| O12 | Belastung der Fahrzeugsignale durch Optokoppler im Messmodus | vor dem Anschliessen mit Multimeter prüfen (Daniel) |
| O13 | Startzeit > 15 s mit Pi OS Lite | Buildroot-Image als eigener Meilenstein nach M8 prüfen |
| O14 | Reicht der SM-I-009 für das Geschwindigkeitssignal (Pulsfrequenz)? | Flanken zählen; sonst T32b/3 über Optokoppler an einen Pi-GPIO mit Interrupt |
