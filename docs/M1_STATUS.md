# M1 status

Stand: 30. Oktober 2026

## Erledigt (läuft ohne Hardware)

| Teil | Ort | Prüfung |
| --- | --- | --- |
| Profil-Loader, unbekannte Schlüssel → Fehler, `TODO_MEASURE` → «nicht kalibriert» | `camperd/profile/` | `tests/test_profile.py` |
| Signale: Entprellen, Median, Flankenzähler, Widerstand ratiometrisch, Steinhart-Hart, Tankstufen mit Hysterese, Pulsdecoder Kühlbox | `camperd/signals/` | `tests/test_signals.py` |
| Module Dach, Kühlbox, Heizung, Tanks, Temperaturen, Fahrzeug, Energie | `camperd/modules/` | `tests/test_modules.py` |
| Power-Zustände drive/afterrun/standby/shutdown | `camperd/power/manager.py` | S8-Tests |
| Watchdog S10 (eigener Thread) | `camperd/power/watchdog.py` | S10-Tests |
| `sim`-HAL mit Fahrzeugmodell, Cerbo-Simulator, Steuerseite `/sim` | `camperd/hal/sim.py`, `ui/sim.html` | alle Rig-Tests |
| WebSocket-API nach SPEC 4 | `camperd/api/` | `tests/test_api.py` |
| Oberfläche aus Prototyp v2, an den Socket gebunden, Texte in `ui/i18n/de.json` | `ui/` | Playwright-Durchlauf, siehe unten |
| Sicherheitsregeln S1–S10 | `tests/safety/test_s*.py` | 60 Tests grün |
| Image-Skript, systemd-Units, `config.txt`, Netzwerk, `make deploy` mit `/data/releases` | `deploy/` | nur Syntax geprüft, nicht auf dem Pi |

End-to-end mit headless Chromium gegen `make dev`: Zustandswechsel im Simulator erscheint nach ~50 ms in der Oberfläche (Ziel ≤ 200 ms). Dach: Loslassen stoppt sofort; bleiben die Haltepulse aus, stoppt das Relais nach 300 ms.

## Offen in M1 (braucht den Pi)

- Plattform-Prüfung: `deploy/build-image.sh` auf frischem Pi OS Lite, `cog --platform=drm` mit Viewe + Touch (O1), HDMI-Modus in `config.txt`, Kaltstartzeit messen (Ziel ≤ 15 s), RAM messen.
- `requirements.lock` mit Hashes erzeugen (auf dem Pi bzw. für aarch64), z. B. mit `pip-compile --generate-hashes`.

## Abweichungen und Annahmen (bitte bestätigen)

1. **Watchdog als Thread statt asyncio-Task (S10).** Ein asyncio-Task läuft nicht, wenn die Event-Loop hängt, und kann sie deshalb nicht überwachen.
2. **Dach-Richtung unbestätigt (O6).** Solange `outputs.roof_a.meaning` nicht gemessen ist, gilt K1 = Öffnen (wie im Prototyp). Während der Fahrt sind dann **beide** Richtungen gesperrt, weil «Schliessen» in Wirklichkeit öffnen könnte. Sobald `meaning` im Profil steht, ist Schliessen während der Fahrt wieder erlaubt (S4).
3. **Zusätzliche Nachricht `release`** von der Oberfläche beim Loslassen der Dachtaste, damit das Dach sofort stoppt und nicht erst nach 300 ms. Der 300-ms-Timeout bleibt als Absicherung.
4. **Zusätzliche Nachricht `touch`** (höchstens 1/s), damit `camperd` Bedienung für Nachlauf/Standby kennt.
5. **Zusätzlicher Cerbo-Pfad** `system/0/Dc/System/Power` für die Zeile «12-V-Verbraucher» des Prototyps. «Lichtmaschine / DC-DC» bleibt «–», weil kein Pfad definiert ist.
6. **Mindestlaufzeit Kühlbox gilt auch beim Ausschalten** durch Bedienung oder Fühlerfehler (Kompressorschutz). Ausschalten kann so bis 2 min verzögert sein.
7. **Heizungs-Sollwertleiter stromlos, solange die Heizung aus ist** (1750 Ω), Relais schalten erst beim Einschalten auf die Stufe.
8. **Warnung `roof_open_ignition`** nur bei bekannter Stellung ≠ geschlossen, nicht bei «unbekannt» (sonst dauernd aktiv, bis die Wahrheitstabelle gemessen ist).
9. **Positionsbalken auf der Dachseite entfernt**: J768 meldet nur Zustände, keine Position.
10. **Module tanks/climate/vehicle** liegen gemeinsam in `camperd/modules/sensors.py`.
11. **Shutdown** wird in M1 nur geloggt; `SHUTDOWN` an die Wake-MCU und `poweroff` folgen mit M6.
12. **Simulator-Kalibrierung** `profiles/sim/sim-calibration.yaml` enthält erfundene Werte, ausschliesslich für `--hal sim`. Das Fahrzeugprofil bleibt unverändert.

## `# OPEN:`-Stellen im Code

```
camperd/modules/roof.py      O6  K1 = open angenommen
camperd/modules/fridge.py        fallback_duty ≠ off nicht umgesetzt
camperd/modules/sensors.py   O8  D+ nur geloggt; O5 Landstrom A11 nur geloggt
camperd/core.py              M6  Shutdown nur geloggt; Aussentemperatur-Quelle cerbo ohne Pfad
camperd/api/store.py         O10 Helligkeit ohne Wirkung
deploy/                      O1  cog/DRM, HDMI-Modus
```

## Nächster Schritt

M2: siehe `docs/M2_STATUS.md` (zwei SM-H-001 statt SM-I-009 + Purecrea).
