# Cali Camping Unit – Software-Spezifikation

Version 1.2 | 30. Oktober 2026 | Fahrzeug: VW T6 California 2.0 TDI 4Motion, MJ 2018, Campingausrüstung A8L/A8R

Version 1.1: Analogeingänge 0–3,3 V mit Referenz aus der Karte, Dachfreigabe nur mit Zündung, Freigabe je Schaltfunktion im Profil (Messung ausstehend), 58d evtl. PWM, T32b/29 = Laderelais-Befehl J713.

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

**Nicht im Umfang v1:** CAN/LIN zum Fahrzeug, Zugriff aus dem Internet, Schreiben auf den Cerbo ausser MultiPlus-Modus und Landstrom-Begrenzung (5.1), Licht-Dimmer, Batterie-Management.

**Licht-Dimmer:** T32b hat in A8L/A8R (Nr. 133) keinen Lichtausgang; Pin 27 / T6ap/4 (58d) ist ein Eingang. Mit 9CE/9CI können Nr. 145/146 weitere Anschlüsse bringen. Dimmen ginge später nur mit eigener Verdrahtung, z. B. über die vier Open-Drain-PWM-Ausgänge des SM-H-001 (24 V, 3 A).

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

Entscheid (HARDWARE.md 5.3), NetworkManager (`deploy/network/setup-network.sh`):

- **Cerbo-Ethernet** bleibt am Camper-Router (unverändert; VRM, Remote Console).
- **Pi `wlan0` → Access Point des Cerbo** (immer an, SSID «venus-…», Passwort auf dem Cerbo-Etikett). Nie Standardroute (`ipv4.never-default`), IPv6 aus, Autoconnect ohne Limit, WLAN-Stromsparen aus. Cerbo-Adresse im AP-Netz erwartet `172.24.24.1` (O19).
- **Pi `eth0` → LAN-Port des Routers** (Router nur zeitweise an): DHCP, Routenmetrik 100; Standardroute nur, solange der Router da ist. Für SSH und Fernzugriff (optional Tailscale).
- Der Cerbo kann zwischen WLAN und Ethernet weder bridgen noch routen (Venus-OS-Kernel ohne Bridge). Nicht versuchen.
- **Keine Steuerfunktion hängt am Router-Netz.** `camperd` braucht für Dach, Kühlbox und Heizung gar kein Netz; Victron-Daten kommen nur über den AP.
- Firewall (`deploy/network/nftables.conf`): auf `wlan0` keine neuen eingehenden Verbindungen (der AP ist mit dem Etikett-Passwort für jeden offen). `sshd` nur mit Schlüssel, erreichbar über `eth0`.
- `config.txt`: `dtoverlay=disable-bt`, WLAN bleibt an.
- **Einstellen am Gerät:** Expertenansicht → Netzwerk (Ethernet DHCP/statisch mit Adresse/Präfix, Gateway, DNS; WLAN-SSID und -Passwort zum Cerbo-AP; Cerbo-Adresse). Eingabe über eine Bildschirmtastatur. `camperd` prüft die Werte und speichert sie in `/data/network.json` (Rechte 600; das Passwort geht nie an die Oberfläche und nie ins Ereignislog). Der Root-Dienst `camper-network` wendet sie mit `nmcli` an, bei jedem Start (Root-Dateisystem schreibgeschützt) und auf Anforderung; `camperd` darf per polkit nur diesen einen Dienst starten. Eine neue Cerbo-Adresse übernimmt der MQTT-Client ohne Neustart. Erstinbetriebnahme ohne Bildschirm: `deploy/network/setup-network.sh`.

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
| `hal/` | Treiber pro Quelle: `sim`, `sm_h001`, `sm_i009`, `hw` (beide Karten + Leiter-GPIO + ALIVE), `cerbo_mqtt`, `wake_uart`. Einheitliche Schnittstelle: `read_di()`, `read_ai()`, `read_rail()` (3,3-V-Schiene SM-H-001), `read_counters()`, `write_relay(ch, on)`, `write_gpio(ch, on)`, Ereignisse als `asyncio.Queue`. |
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

## 5. Victron Cerbo GX (MQTT)

Verkabelung: MultiPlus Compact → Cerbo über **VE.Bus** (RJ45-Patchkabel an die VE.Bus-Buchse des Cerbo, nicht an dessen Ethernet). Pi ↔ Cerbo über Ethernet.

- Verbindung `172.24.24.1:1883` (AP des Cerbo, `buses.cerbo.host`), Client-ID `camperd`, MQTT-Ping 10 s. Reconnect ohne manuellen Schritt nach Neustart oder Firmware-Update des Cerbo: Backoff 1–10 s; nach jedem Verbinden Portal-ID und VE.Bus-Instanz neu ermitteln und alle Datenthemen neu abonnieren. Offene Schreibbefehle verfallen beim Verbindungsabbruch.
- Portal-ID beim Start aus `N/+/system/0/Serial` ermitteln. VE.Bus-Instanz aus `N/<id>/vebus/+/ProductId`.
- Keepalive: `R/<id>/keepalive` mit **leerem** Payload alle `buses.cerbo.keepalive_s` = 5 s. Venus publiziert nur, solange Keepalives kommen, und sendet bei leerem Keepalive alle Werte neu. Weil Werte sonst nur bei Änderung kommen, muss das Intervall ≤ `stale_s` / 2 sein (Code prüft das), damit ein gleichbleibender Wert nicht als veraltet gilt. Optional feste Portal-ID `buses.cerbo.portal_id`, falls `N/+/system/0/Serial` nicht ankommt.
- Gelesen:
  - `system/0/Dc/Battery/{Soc,Voltage,Current,Power,TimeToGo}` (TimeToGo nur mit Batteriemonitor), `system/0/Dc/Pv/Power`, `system/0/Dc/Charger/Power`, `system/0/Dc/Alternator/Power` (Orion XS, Laden während der Fahrt)
  - `vebus/<inst>/{State,Mode,Ac/ActiveIn/ActiveInput,Ac/ActiveIn/L1/{V,I,F,P},Ac/Out/L1/{V,I,F,P,S},Ac/PowerMeasurementType,Dc/0/Power}`
  - `vebus/<inst>/Alarms/{L1/Overload,LowBattery,HighTemperature,L1/Ripple}`
  - `vebus/<inst>/Ac/In/1/{CurrentLimit,CurrentLimitIsAdjustable}`, `system/0/Dc/System/Power`
  - `solarcharger/+/State`, `alternator/+/State`, `dcdc/+/State`: Ladephase von MPPT und Orion XS (0 aus, 2 Fehler, 3 Bulk, 4 Absorption, 5 Erhaltung, 6 Lagerung, 7 Ausgleich). Ladeleistung des MultiPlus aus `vebus/<inst>/Dc/0/Power` (positiv = lädt); `system/0/Dc/Charger/Power` zählt nur separate Ladegeräte, nicht den MultiPlus
  - `alternator/+/Dc/In/V`, `dcdc/+/Dc/In/V`: Eingangsspannung der Orion XS = **Starterbatterie** (nur Anzeige; bei mehr als einem DC-DC-Lader «–»)
  - `temperature/+/{Temperature,Humidity,CustomName,ProductName,TemperatureType}`: Innentemperatur (5.2)
- Jeder Wert mit Empfangszeit. Alter > 10 s (`stale_s`) oder Verbindung weg → Oberfläche zeigt «–», **nie den letzten Wert**. Fehlender Wert → `null`, nie ein Ersatzwert.
- Auf `W/…` nur nach 5.1.

### 5.1 MultiPlus steuern (Version 1.2)

Nur mit `victron.control: true` im Profil (Standard `false`). Erlaubt sind genau zwei Schreibzugriffe, beide nur auf Bedienung, nie in einer Schleife:

| Pfad | Werte | Bedeutung |
| --- | --- | --- |
| `W/<id>/vebus/<inst>/Mode` | 1, 3, 4 (Liste `victron.modes`) | 1 nur Laden (Schalter II), 3 ein (I), 4 aus (O); 2 «nur Wechselrichter» nur, wenn im Profil gelistet |
| `W/<id>/vebus/<inst>/Ac/In/1/CurrentLimit` | `victron.current_limit_a.min`–`max` | Landstrom-Begrenzung in A; nur wenn `CurrentLimitIsAdjustable` = 1 |

- Payload `{"value": <wert>}`. Der Client lehnt jeden anderen Pfad ab (Test).
- Höchstens ein Schreibzugriff je `victron.min_interval_s` (2 s).
- Rücklesen: Zeigt der Cerbo den neuen Wert nicht innert `victron.confirm_s` (5 s), Meldung «MultiPlus hat die Änderung nicht übernommen» (z. B. Frontschalter nicht auf I, VE.Bus-BMS, Panel).
- Oberfläche: Expertenansicht → MultiPlus → Steuerung. Jede Änderung zweimal tippen; «Aus» trennt 230 V und das Laden.
- Dach, Kühlbox und Heizung hängen weiterhin nicht am Cerbo (S9).

---

### 5.2 Innentemperatur (Ruuvi-Sensor, nur Anzeige)

- Ein Ruuvi-Tag im Wohnraum sendet per Bluetooth an den Cerbo; Venus OS publiziert ihn als Temperaturdienst `temperature/<inst>`. Keine Verdrahtung zur Unit.
- Auswahl über `interior_temp.custom_name` (der am Cerbo vergebene Name, Gross-/Kleinschreibung egal). Steht dort `TODO_CHECK`: der einzige Ruuvi-Tag (`ProductName` enthält «Ruuvi»); bei mehreren Tags keine Anzeige und Hinweis «Mehrere Ruuvi-Sensoren» (O20). Nie raten.
- Anzeige im Kopf («Innen») und auf der Seite Heizung (mit Luftfeuchte). Gleiche Altersregel wie oben: älter als `stale_s` → «–».
- Nur Anzeige. Die Standheizung regelt weiter mit ihrem eigenen Fühler; keine Schaltlogik hängt am Wert.
- `interior_temp.source: cerbo` schaltet die Anzeige ein; jeder andere Wert blendet sie aus.

## 6. Sicherheitsregeln (mit Tests, vor jeder Hardware-Freigabe)

| Nr. | Regel |
| --- | --- |
| S1 | Beim Start sind K1–K8 aus und H1–H4 stromlos (NC geschlossen = Basiswiderstand R0), bevor `camperd` irgendeine Nachricht annimmt. Der Einschaltzustand der Relais des SM-H-001 (Register «relay default») wird auf 0 gesetzt; der Bootpegel von H1–H4 steht in `config.txt`. |
| S2 | K1 und K2 nie gleichzeitig. Richtungswechsel nur nach 300 ms mit beiden aus. |
| S3 | Dachrelais fällt ab, wenn 300 ms keine Laufmeldung (`hold`) kommt, der WebSocket abreisst oder 60 s Laufzeit pro Befehl erreicht sind (Wert im Profil). Gilt für beide Bedienarten: Halten und Antippen (`auto`). Beim Antippen sendet die Oberfläche die Laufmeldung selbst, bis STOPP, irgendeine Berührung, die Endlage oder eine Ablehnung; `camperd` stoppt zusätzlich selbst in der Endlage. Antippen nur mit gemessener Wahrheitstabelle und Relaisrichtung (`roof.tap_to_run`), sonst Halten. |
| S4 | Öffnen des Dachs gesperrt, solange Geschwindigkeitsimpulse anliegen (Profil: `inputs.speed`). Schliessen bleibt erlaubt. Läuft ein Öffnen-Befehl, wenn die Fahrt beginnt: sofort stoppen. Wie beim Original-E153 fährt das Dach nur mit Zündung ein (`roof.require_ignition`); fällt die Zündung ab: sofort stoppen. |
| S5 | Fällt der SM-I-009 aus (3 I²C-Fehler in Folge): DI-Werte `stale`, Dach gesperrt, Meldung. Schaltet ein Relais des SM-H-001 laut Rücklesen nicht: Fehler. |
| S6 | Kühlbox: Mindestlaufzeit 2 min, Mindestpause 3 min (Profil). |
| S7 | Heizungsstufe: H-Relais nur einzeln umschalten, 50 ms Abstand. Nie zwei gleichzeitig. |
| S8 | Kein Shutdown, solange die Heizung im Nachlauf ist oder das Dach fährt bzw. die Pumpe nachläuft (`roof.pump_afterrun_s`). |
| S9 | Ausfall des Cerbo ändert nichts an Dach, Kühlbox und Heizung. |
| S10 | Ein Watchdog im `camperd` (eigener Thread, weil ein asyncio-Task eine hängende Event-Loop nicht bemerkt) prüft den Hauptzyklus. Hängt er > 2 s: alle Relais aus, Prozess beenden, systemd startet neu. |

Jede Regel hat mindestens einen Test in `tests/safety/`, benannt nach der Nummer (`test_s3_roof_hold_timeout.py`).

**Freigabe je Schaltfunktion:** `release.roof`, `release.fridge`, `release.heater` im Profil. Solange `false` (Messung ausstehend), schaltet `camperd` die Ausgänge dieser Funktion nie und lehnt ihre Befehle mit `not_released` ab. Freigabe erst nach Messung und Test am Fahrzeug (M7). Test: `tests/safety/test_release_gate.py`.

---

## 7. Funktionsmodule

### 7.1 Dach (`roof`)

- Eingänge: DI1 (T32b/15), DI2 (T32b/14), Geschwindigkeit DI6. Ausgänge: K1 (T32b/11), K2 (T32b/12).
- Welche Richtung K1/K2 bedeutet und wie DI1/DI2 die Stellung kodieren, kommt aus der Wahrheitstabelle im Profil (`roof.truth_table`, `TODO_MEASURE`). Ohne Tabelle: Stellung `unknown`, Bedienung trotzdem möglich (Hold-to-run).
- Zustände: `closed`, `open`, `moving`, `between`, `unknown`, `fault`.
- Warnung `roof_open_ignition`, wenn Zündung an und Dach nicht `closed`.
- Freigabe nur mit Zündung ein und Fahrzeug steht (Original-E153). Welcher Befehl öffnet, ist unbestätigt (O6).
- **Bedienung (Entscheid 31.10.2026):** ein Tippen fährt in beide Richtungen bis zur Endlage; grosse STOPP-Taste, jede Berührung des Bildschirms stoppt. Bis Endlagen und Richtung gemessen sind: Halten (Hold-to-run).
- **Pumpennachlauf:** Nach jedem Stopp gilt das Dach für `roof.pump_afterrun_s` als in Bewegung (kein Richtungswechsel, kein Shutdown, keine Rückkehr zur Übersicht; Anzeige «Pumpe läuft nach»). Wert `TODO_MEASURE`; bis dahin gilt nur die Umkehrpause 300 ms (`# OPEN:`).

**Abnahme:** S2, S3, S4 als Tests grün (`tests/safety/test_s3b_roof_tap_and_afterrun.py` für Antippen und Nachlauf). Im Simulator: Antippen → fährt bis zur Endlage und stoppt; STOPP oder Berührung → innert 300 ms aus; WebSocket trennen → innert 300 ms aus; Nachlaufanzeige nach dem Stopp.

### 7.2 Kühlbox (`fridge`)

- Thermostat auf G817 (AI1): Anforderung an bei `temp ≥ target + 1.5`, aus bei `temp ≤ target − 1.0` (Profil). Soll 1–10 °C.
- Diagnose an DI3 (T32b/9): low-aktive Pulse 0,25 s, Sequenz alle 4 s. Code erst nach zwei gleichen Sequenzen melden. BD35F-Standard: Codes 1–5. Code 6 wird angezeigt, aber als «Bedeutung unbestätigt» (VW-Auslegung, testen).
- G817 `open`/`short`/`uncalibrated` → Thermostat aus, Meldung. Option `fallback_duty` im Profil (Standard aus).
- Oberfläche sagt «Kühlung angefordert», nicht «Kompressor läuft».

**Abnahme:** S6 als Test; Pulsdecoder-Test mit aufgezeichneten Pulsfolgen für Codes 1–6 und mit Störpulsen.

### 7.3 Standheizung (`heater`)

- K4 = S+ (T6ap/3, bestätigt). DI4 = Betriebsanzeige (T6ap/2).
- Stufe → H1–H4 binär (H1 = Bit 0 … H4 = Bit 3). Relais an = NC offen = Segment wirksam. Widerstand = R0 + Σ Segmente. R0 und Segmente `TODO_MEASURE` (1750 / 27,4 / 54,9 / 110 / 237 Ω unbestätigt); bis dahin bleibt die Leiter stromlos und die Stufe ist nicht einstellbar.
- **10 Stufen** wie am Original (`heater.levels: 10`, bestätigt; SSP 329: +10 bis +30 °C). Intern 0–9 → H1–H4 binär (4 Relais, 10 von 16 Kombinationen genutzt); die Oberfläche zählt 1–10.
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
- Innentemperatur: Ruuvi-Tag über den Cerbo (5.2), nur Anzeige.

### 7.6 Fahrzeug (`vehicle`)

15a (DI10), 86s (DI8, bestätigt über B518), Laderelais-Befehl J713 / Motor läuft (DI9, T32b/29 über B478 an J519 T73a/3 und Spule J713; kein rohes D+; nur loggen), 58d (DI7, Nachtmodus; evtl. PWM, deshalb «hoch innerhalb `inputs.dim_58d.window_ms`» = an), Landstrom A11 (DI5, nur loggen, bis geklärt ist, ob A11 noch eingebaut ist).

Geschwindigkeit (DI6): nur «fährt / steht», nicht km/h. Der Flankenzähler des SM-I-009 (nur Hardware V3, beide Flanken) liefert die Flanken im Fenster von 1 s; ab `inputs.speed.moving_min_edges` gilt «fährt». Auf V2-Karten zählt die Software Pegelwechsel bei 20-ms-Abfrage.

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
- **Navigation:** Übersicht (5 Kacheln: «Batterie und Strom» doppelt breit, Kühlbox, Heizung, Wasser, Dach) → Detailseite → «‹ Übersicht». Zahnrad → Einstellungen → «Expertenansicht» → Reiter Signale / Stecker T32b / MultiPlus / Verbindung / Netzwerk / Messmodus. Keine weiteren Menüpunkte.
- **Rückkehr zur Übersicht** nach 1/2/5 min ohne Bedienung (Einstellung, Standard 2 min), nie während das Dach fährt.
- **Tastengrössen:** Plus/Minus 112 px, Ein/Aus 88 px hoch, Dachtasten volle Höhe, sonst mindestens 52 px.
- **Farben:** eine Akzentfarbe; Grün/Gelb/Rot nur für Zustände. Tag- und Nachtschema, Automatik über 58d.
- **Texte** aus `ui/i18n/de.json` (Standard) und `ui/i18n/en.json`. Umschaltbar unter Einstellungen → Bedienung → Sprache (Einstellung `language`, Profil-Standard `ui.language`); Zahlen und Uhrzeit folgen der Sprache. Keine Texte im JS.
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
| O4 | Belegt der SM-H-001 UART oder weitere GPIO? | erledigt: SM-H-001 und SM-I-009 nutzen nur I²C (GPIO2/3) |
| O5 | Ist A11 noch eingebaut? | DI5 nur loggen |
| O6 | Wahrheitstabelle Dach, Pegel aller Ein-/Ausgänge | aus Messmodus (M3/M4) |
| O7 | Kennlinien G817, G249, G120, G126, Heizungs-Sollwert | `TODO_MEASURE`, Anzeige «nicht kalibriert» |
| O8 | T32b/29 = Laderelais-Befehl J713 (Motor läuft), kein rohes D+ | nur loggen; Pegel messen |
| O9 | Touch-Controller mit IRQ für Wake? | ohne IRQ: Wake nur über Zündung |
| O10 | Helligkeitssteuerung der Viewe-Treiberplatine | Regler ohne Wirkung, Hinweis in der Oberfläche |
| O11 | Unterspannungsschwelle | 11,8 V für 60 s |
| O12 | Belastung der Fahrzeugsignale durch Optokoppler im Messmodus | vor dem Anschliessen mit Multimeter prüfen (Daniel) |
| O13 | Startzeit > 15 s mit Pi OS Lite | Buildroot-Image als eigener Meilenstein nach M8 prüfen |
| O14 | Reicht der SM-I-009 für das Geschwindigkeitssignal (Pulsfrequenz)? | V3: Hardware-Zähler; V2: Flanken bei 20 ms zählen, sonst T32b/3 über Optokoppler an einen Pi-GPIO mit Interrupt. Frequenz messen |
| O15 | Sollwertleiter: Widerstand je Stufe (10 Stufen, bestätigt), Richtung | Leiter stromlos, Stufe nicht einstellbar, Heizung nicht freigegeben |
| O16 | 15-kΩ-Pull-up auf dem SM-H-001 bestückt? Eingangsimpedanz je Revision | `ai_input_ohm: TODO_CHECK`; offene AI-Spannung messen |
| O17 | Mindestlast der Relaiskontakte bei Signalstrom (K1–K3, H1–H4); Polarität Purecrea-Eingänge | Datenblatt; `gpio_active: TODO_CHECK`, H1–H4 nicht angesteuert |
| O18 | Akzeptieren J768, J604, J1029 einen Relaiskontakt statt des überwachten E153-Halbleiterausgangs? | erst nach Freigabe am Fahrzeug testen |
| O19 | **Blockierend:** Antwortet MQTT des Cerbo über dessen WLAN-AP? Adresse im AP-Netz (erwartet 172.24.24.1)? | `tools/cerbo_check.py`. Antwortet MQTT nicht: STOPP und melden; Rückfall unmanaged Switch (HARDWARE.md 5.3) |
| O20 | Name des Ruuvi-Tags am Cerbo; meldet Venus einen verstummten Tag (Dienst verschwindet) oder publiziert er den letzten Wert weiter? | `custom_name: TODO_CHECK` → einziger Ruuvi-Tag; am Fahrzeug Batterie aus dem Tag nehmen und prüfen, ob die Anzeige nach `stale_s` auf «–» geht |
| O21 | Füllt Venus 3.85 `system/0/Dc/Alternator/Power` für die Orion XS (in VRM als «Alternator»)? | erledigt (Daniel, 31.10.2026): ja, der Wert kommt |
