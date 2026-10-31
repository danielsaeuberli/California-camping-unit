# CLAUDE.md – Cali Camping Unit

Ersatz für das Camping-Bedienteil E153 im VW T6 California (MJ 2018, Ausstattung A8L/A8R).

**Architektur (Entscheid 31.10.2026, SPEC 2.0): «XIAO regelt, Pi nur zur Bedienung».** Eine immer laufende XIAO ESP32-S3 ist der einzige Master für alle Ausgänge (Dach, Kühlbox, Standheizung), liest die Relais- und Eingangskarten über I²C, ohne WLAN und ohne MQTT. Ein Raspberry Pi 5 mit 1280 × 480-Touchdisplay ist nur Bedienterminal, läuft nur bei Bedienung oder Zündung, schickt Wünsche per UART an die XIAO und holt die Victron-Anzeige selbst über das WLAN des Cerbo. Ziel: geparkt mit Regelung 0,5–1 W.

## Zuerst lesen

1. `docs/SOFTWARE_SPEC.md` – was gebaut wird, Schnittstellen, Abnahmekriterien, Meilensteine
2. `docs/HARDWARE.md` – Komponenten, Kanalbelegung, elektrische Randbedingungen (Kapitel 0: Architektur)
3. `docs/UART_PROTOCOL.md` – Pi ↔ XIAO
4. `profiles/t6-california-a8l-2018.yaml` – Zuordnung VW-Pin → Kanal (Quelle der Wahrheit für die Verdrahtung)
5. `ui/reference/Camping_Unit_Oberflaeche_v2.html` – abgenommenes Oberflächenkonzept (Klick-Prototyp mit Demo-Daten)
6. Bei Fragen zu Pins: `docs/reference/E153_Pin_Check_Nr133.md` und `docs/reference/Recherche_Camping_Unit.md`

## Regeln

- **Sicherheit vor Funktion.** Dach (Hydraulik), Standheizung und Kühlbox sind echte Fahrzeuggeräte. Jede Schaltlogik braucht Unit-Tests für die Sicherheitsregeln in SPEC Kapitel 6, bevor sie auf Hardware läuft.
- **Der Pi schaltet nie.** Alle Ausgänge und alle Sicherheitsregeln (SPEC 6) liegen in der XIAO-Firmware. Der Pi hat keinen Zugriff auf den Sequent-I²C (`dtparam=i2c_arm=off`). Victron: Der Pi liest für die Anzeige und bedient den MultiPlus (SPEC 5.1); die XIAO spricht nicht mit dem Cerbo (Unterspannung über AI8). Die Python-Logik in `camperd` (hal, signals, modules) ist Referenzmodell und Simulator für die Firmware.
- **Nicht bauen, bevor gemessen:** Pegel und Polarität T32b/10/11/12 und T6ap/3, Wahrheitstabelle T32b/14/15, T2p je Heizstufe, Kennlinien G817/G249/G120/G126, T32b/29, MQTT über den AP des Cerbo (HARDWARE.md 6).
- **Keine Rückspeisung in den ausgeschalteten Pi:** Signale zum Pi nur über Puffer mit Partial-Power-Down (HARDWARE.md 4.4).
- **Simulator ist Standard.** `camperd` startet ohne Konfiguration mit dem `sim`-Treiber. Echte Hardware nur mit `--hal hw` oder `CAMPERD_HAL=hw`.
- **Keine erfundenen Messwerte.** Alles, was im Profil mit `TODO_MEASURE` markiert ist, bleibt so. Code muss mit fehlenden Kennlinien umgehen («nicht kalibriert»), nicht mit Schätzwerten.
- **Profil statt Code.** Pinbelegung, Polaritäten, Schwellen, Zeiten und Kennlinien stehen im YAML-Profil, nicht im Code.
- **Cerbo: lesen, schreiben nur eng begrenzt.** `W/…` nur für `vebus/<inst>/Mode` (1/3/4) und `vebus/<inst>/Ac/In/1/CurrentLimit`, nur auf Bedienung, nur mit `victron.control: true` im Profil (SPEC 5.1). Keine anderen `W/…`-Themen.
- **Offene Punkte nicht still entscheiden.** SPEC Kapitel 12 listet sie. Wenn eine Umsetzung davon abhängt: Standardwert aus der SPEC nehmen, im Code als `# OPEN:` markieren und im PR erwähnen.
- Bezeichner, Code-Kommentare und Commit-Messages auf Englisch. Texte der Oberfläche über Sprachdateien: Deutsch (Standard, Schweizer Orthografie, kein «ß») in `ui/i18n/de.json`, Englisch in `ui/i18n/en.json`. Jeder neue Text in beide Dateien (Test prüft gleiche Schlüssel).

## Stack (so schlank wie möglich)

- **OS:** Raspberry Pi OS Lite 64 bit (Bookworm), ohne Desktop, ohne X11, ohne Compositor. Root-Dateisystem schreibgeschützt (overlayfs). Details SPEC Kapitel 2.
- **XIAO-Firmware** (`xiao/`): ESP-IDF über PlatformIO. Regel- und Sicherheitslogik in reinem C ohne ESP-IDF-Abhängigkeit, Uhr injiziert, Tests auf dem Rechner (`pio test -e native`, Unity). WLAN, Bluetooth und MQTT bleiben aus.
- **Ein Dienst auf dem Pi:** `camperd` (Python 3.11, asyncio) liefert die Oberfläche als statische Dateien aus, spricht mit ihr per WebSocket (`aiohttp`), mit der XIAO per UART (`docs/UART_PROTOCOL.md`) und mit dem Cerbo per MQTT (`aiomqtt`, Anzeige und MultiPlus). Kein lokaler MQTT-Broker.
- **Oberfläche:** statisches HTML/CSS/JS ohne Build-Schritt und ohne Framework, angezeigt mit `cog` (WPE WebKit) auf der DRM-Plattform. Kein Chromium.
- **Python-Abhängigkeiten** nur: `aiohttp`, `aiomqtt`, `smbus2`, `gpiod`, `pyserial-asyncio`, `pyyaml`. Jede weitere Abhängigkeit begründen.
- **Tests:** `pytest`, `pytest-asyncio` auf dem Pi-/Python-Teil, Unity in der Firmware; Zeit in Zustandsmaschinen über eine injizierbare Uhr, damit Tests ohne Warten laufen.

## Befehle (Ziel, beim Aufsetzen anlegen)

```
make dev        # camperd mit sim-HAL, UI auf http://localhost:8080 (läuft auf dem Mac)
make test       # pytest
make lint       # ruff + mypy
make deploy     # rsync auf den Pi (Overlay kurz aus), Dienst neu starten
```

## Repository

```
camper-unit/
  CLAUDE.md
  docs/            SOFTWARE_SPEC.md, HARDWARE.md, UART_PROTOCOL.md, reference/
  profiles/        t6-california-a8l-2018.yaml
  camperd/         api/ link/ profile/ net.py main.py; hal/ signals/ modules/ power/ = Referenzmodell + Simulator
  ui/              index.html, app.js, style.css, i18n/de.json, reference/
  xiao/            Firmware (ESP-IDF, PlatformIO): components/ test/
  tools/           Messmodus-Auswertung, CSV-Plots
  tests/
  deploy/          image-Skript, systemd-Units, cog-Kiosk, config.txt, Netzwerk
```

## Reihenfolge

Meilensteine M1 bis M8 in SPEC Kapitel 11. M1 und M2 sind erledigt. Als Nächstes M3 (Messmodus) und M5 (XIAO-Grundgerüst). Schaltfunktionen (M7) erst nach den Messungen (M4).
