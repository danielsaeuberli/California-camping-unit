# CLAUDE.md – Cali Camping Unit

Ersatz für das Camping-Bedienteil E153 im VW T6 California (MJ 2018, Ausstattung A8L/A8R). Ein Raspberry Pi 5 mit Relais- und Eingangskarten übernimmt die Bedienung von Dach, Kühlbox und Standheizung, zeigt Tanks, Temperaturen und Victron-Daten auf einem 1280 × 480-Touchdisplay an und verwaltet den Strom der Unit.

## Zuerst lesen

1. `docs/SOFTWARE_SPEC.md` – was gebaut wird, Schnittstellen, Abnahmekriterien, Meilensteine
2. `docs/HARDWARE.md` – Komponenten, Kanalbelegung, elektrische Randbedingungen
3. `profiles/t6-california-a8l-2018.yaml` – Zuordnung VW-Pin → Kanal (Quelle der Wahrheit für die Verdrahtung)
4. `ui/reference/Camping_Unit_Oberflaeche_v2.html` – abgenommenes Oberflächenkonzept (Klick-Prototyp mit Demo-Daten)
5. Bei Fragen zu Pins: `docs/reference/E153_Pin_Check_Nr133.md` und `docs/reference/Recherche_Camping_Unit.md`

## Regeln

- **Sicherheit vor Funktion.** Dach (Hydraulik), Standheizung und Kühlbox sind echte Fahrzeuggeräte. Jede Schaltlogik braucht Unit-Tests für die Sicherheitsregeln in SPEC Kapitel 6, bevor sie auf Hardware läuft.
- **Simulator ist Standard.** `camperd` startet ohne Konfiguration mit dem `sim`-Treiber. Echte Hardware nur mit `--hal hw` oder `CAMPERD_HAL=hw`.
- **Keine erfundenen Messwerte.** Alles, was im Profil mit `TODO_MEASURE` markiert ist, bleibt so. Code muss mit fehlenden Kennlinien umgehen («nicht kalibriert»), nicht mit Schätzwerten.
- **Profil statt Code.** Pinbelegung, Polaritäten, Schwellen, Zeiten und Kennlinien stehen im YAML-Profil, nicht im Code.
- **Cerbo: lesen, schreiben nur eng begrenzt.** `W/…` nur für `vebus/<inst>/Mode` (1/3/4) und `vebus/<inst>/Ac/In/1/CurrentLimit`, nur auf Bedienung, nur mit `victron.control: true` im Profil (SPEC 5.1). Keine anderen `W/…`-Themen.
- **Offene Punkte nicht still entscheiden.** SPEC Kapitel 12 listet sie. Wenn eine Umsetzung davon abhängt: Standardwert aus der SPEC nehmen, im Code als `# OPEN:` markieren und im PR erwähnen.
- Bezeichner, Code-Kommentare und Commit-Messages auf Englisch. Texte der Oberfläche über Sprachdateien: Deutsch (Standard, Schweizer Orthografie, kein «ß») in `ui/i18n/de.json`, Englisch in `ui/i18n/en.json`. Jeder neue Text in beide Dateien (Test prüft gleiche Schlüssel).

## Stack (so schlank wie möglich)

- **OS:** Raspberry Pi OS Lite 64 bit (Bookworm), ohne Desktop, ohne X11, ohne Compositor. Root-Dateisystem schreibgeschützt (overlayfs). Details SPEC Kapitel 2.
- **Ein Dienst:** `camperd` (Python 3.11, asyncio) liest die Hardware, führt die Logik, liefert die Oberfläche als statische Dateien aus und spricht mit ihr per WebSocket (`aiohttp`). Kein lokaler MQTT-Broker.
- **MQTT** nur als Client zum Victron Cerbo GX (`aiomqtt`).
- **Oberfläche:** statisches HTML/CSS/JS ohne Build-Schritt und ohne Framework, angezeigt mit `cog` (WPE WebKit) auf der DRM-Plattform. Kein Chromium.
- **Python-Abhängigkeiten** nur: `aiohttp`, `aiomqtt`, `smbus2`, `gpiod`, `pyserial-asyncio`, `pyyaml`. Jede weitere Abhängigkeit begründen.
- **Wake-MCU:** Seeed XIAO ESP32-S3, Arduino-Framework über PlatformIO.
- **Tests:** `pytest`, `pytest-asyncio`; Zeit in Zustandsmaschinen über eine injizierbare Uhr, damit Tests ohne Warten laufen.

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
  docs/            SOFTWARE_SPEC.md, HARDWARE.md, reference/
  profiles/        t6-california-a8l-2018.yaml
  camperd/         hal/ signals/ modules/ api/ power/ main.py
  ui/              index.html, app.js, style.css, i18n/de.json, reference/
  wake-mcu/        platformio.ini, src/
  tools/           Messmodus-Auswertung, CSV-Plots
  tests/
  deploy/          image-Skript, systemd-Units, cog-Kiosk, config.txt, Netzwerk
```

## Reihenfolge

Meilensteine M1 bis M8 in SPEC Kapitel 11. Mit M1 beginnen. Erst wenn M1 lokal ohne Hardware läuft und die Tests grün sind, weiter.
