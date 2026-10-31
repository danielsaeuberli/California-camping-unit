# UART-Protokoll Pi ↔ XIAO

Version 1 | 31. Oktober 2026 | gehört zu SOFTWARE_SPEC 2.0 (Kapitel 3, 6, 10) und HARDWARE.md 1.3 (Kapitel 4.4, 4.5)

Victron-Werte laufen nicht über diese Leitung: Der Pi holt sie selbst über das WLAN des Cerbo (SPEC 2.7, 5).

Die XIAO ist Master für alle Ausgänge. Der Pi schickt nur Wünsche; die XIAO prüft jeden gegen die Sicherheitsregeln (SPEC 6) und antwortet. Fällt die Verbindung weg, regelt die XIAO unverändert weiter (S14); nur das Dach stoppt (S3).

## 1. Leitung

- UART 3,3 V, 115200 Bd 8N1, ohne Flusskontrolle (O28: bei Bedarf 460800 Bd).
- Pi GPIO4/5 (`uart2-pi5`) ↔ XIAO D6/D7. XIAO-TX nur über den vom Pi versorgten Puffer (SN74LVC1G125), Pi-TX über 1 kΩ. Solange der Pi aus ist, schaltet die XIAO ihren TX auf Eingang (keine Rückspeisung).

## 2. Rahmen

Eine Nachricht pro Zeile: kompaktes JSON, ein Leerzeichen, `*`, CRC-16/CCITT-FALSE des JSON-Teils in 4 Hex-Ziffern, `\n`.

```
{"t":"hb","n":17} *3A5F
```

- Höchstens 1024 Byte pro Zeile. Längere Zeilen, falsche CRC oder ungültiges JSON: verwerfen, Fehlerzähler erhöhen (Expertenansicht), keine Antwort.
- Jede Nachricht hat `t` (Typ). Unbekannte Typen: verwerfen, `{"t":"err","reason":"unknown_type"}`.
- Zahlen in SI-Einheiten wie im WebSocket-Protokoll (SPEC 4), Temperaturen in °C, Zeiten in s.

## 3. Sitzung

Beide Seiten haben eine **Startkennung** (`boot`): 32-bit-Zufallszahl, neu bei jedem Start, als 8 Hex-Ziffern.

1. Der Pi sendet nach dem Start und nach jeder Unterbrechung (3 s ohne Nachricht der XIAO) `hello`.
2. Die XIAO antwortet mit `welcome` und sofort einem vollständigen `snapshot`.
3. Erst danach nimmt die XIAO Befehle an. Jeder Befehl trägt beide Startkennungen. Passt eine nicht zur laufenden Sitzung: `ack` mit `ok:false`, `reason:"stale_session"`, nichts ausführen.

```
Pi   → {"t":"hello","proto":1,"pi":"7c1e09a2","sw":"0.9.0"}
XIAO → {"t":"welcome","proto":1,"xiao":"b44f0e13","pi":"7c1e09a2","fw":"0.1.0","profile":"sha256:…"}
XIAO → {"t":"snapshot","state":{…},"settings":{…},"power":{…}}
```

- **Keine Wiederholung alter Befehle (S13):** Der Pi verwirft beim Verbindungsaufbau alle offenen Befehle; die Oberfläche zeigt bis zum `snapshot` «Steuerung offline». Die XIAO merkt sich die höchste ID der Sitzung und lehnt kleinere oder gleiche IDs ab (`reason:"duplicate"`). Nach einem Neustart der XIAO ist die alte Sitzung ungültig.
- Ist `proto` verschieden: XIAO antwortet `{"t":"err","reason":"proto"}` und nimmt keine Befehle an; die Oberfläche zeigt «Firmware passt nicht».

## 4. Nachrichten

### 4.1 Pi → XIAO

| `t` | Felder | Bedeutung |
| --- | --- | --- |
| `hello` | `proto`, `pi`, `sw` | Sitzung beginnen |
| `hb` | `n` (zählt hoch) | jede Sekunde; 2 min ohne `hb` im Zustand `on` → XIAO startet den Pi neu (SPEC 10.1) |
| `cmd` | `id`, `xiao`, `pi`, `target`, `set` | wie `cmd` im WebSocket (SPEC 4): `fridge`, `heater`. Antwort `ack`. MultiPlus-Befehle (`victron`) gehen nicht über die UART: Der Pi schreibt sie selbst an den Cerbo (SPEC 5.1) |
| `roof` | `id`, `xiao`, `pi`, `dir` (`open`/`close`/`stop`), `seq`, `auto` | Laufmeldung alle 100 ms, solange das Dach fahren soll. `seq` steigt streng. Die XIAO stoppt nach 300 ms ohne `roof` (S3), bei `stop`, in der Endlage oder bei jeder Sperre. Nie gepuffert: Eine Laufmeldung, die älter als der letzte Stopp ist (`seq` kleiner), gilt nicht |
| `settings` | `id`, `xiao`, `pi`, `set` | Helligkeit, Darstellung, `idle_min`, Sprache |
| `subscribe` | `topics` (`["io"]`) | Rohwerte schnell senden (Expertenansicht, Messmodus) |
| `config` | `id`, `xiao`, `pi`, `wifi` (`ssid`, `psk`), `cerbo` (`host`, `port`) | WLAN zum AP des Cerbo; XIAO speichert im NVS. `psk` wird nie zurückgeschickt |
| `profile` | `id`, `xiao`, `pi`, `sha256`, `part`, `of`, `data` | Profil als JSON in Teilen ≤ 700 Byte. Die XIAO prüft Hash und Schema, speichert erst vollständig. Änderungen an `release.*` gelten erst nach einer Bestätigung am Gerät (zweimal tippen) |
| `shutdown_ok` | – | Antwort auf `shutdown`: der Pi fährt jetzt herunter |
| `busy` | `s` (≤ 60) | Herunterfahren aufschieben (Update läuft), höchstens 10 min insgesamt |

### 4.2 XIAO → Pi

| `t` | Felder | Bedeutung |
| --- | --- | --- |
| `welcome` | `proto`, `xiao`, `pi`, `fw`, `profile` | Sitzung angenommen |
| `snapshot` | `state`, `settings`, `power` | vollständiger Zustand, nach `welcome` und auf Wunsch |
| `state` | wie `state.data` im WebSocket | bei Änderung, spätestens jede Sekunde |
| `io` | `di`, `k`, `h`, `ai_v`, `rail_v`, `ts_ms` | nur nach `subscribe`, 10 Hz (DI-Flanken mit Zeitstempel im Messmodus) |
| `ack` | `id`, `ok`, `reason` | Antwort auf jeden Befehl mit `id`. Gründe wie im WebSocket (`not_released`, `blocked_moving`, …) plus `stale_session`, `duplicate` |
| `power` | `mode` (`booting`/`on`/`shutting_down`), `display` (`on`/`dark`), `wake` (`touch`/`86s`/`ignition`) | bei Änderung |
| `touch_wake` | – | die letzte Berührung war eine Weck-Berührung: die Oberfläche ignoriert sie (S12) |
| `shutdown` | `reason` (`idle`/`undervoltage`), `in_s` | Pi soll herunterfahren; Antwort `shutdown_ok` |
| `err` | `reason` | Rahmen- oder Protokollfehler |

## 5. Herunterfahren

1. XIAO → `shutdown` (`in_s` = 0 oder Vorwarnung).
2. Pi → `shutdown_ok`, dann `systemctl poweroff`.
3. Der Pi setzt beim Halt GPIO26 (`gpio-poweroff`). Die XIAO wartet darauf, höchstens 60 s, dann 2 s Reserve, dann DCDC-Enable aus.
4. Kommt kein `shutdown_ok` innert 30 s: einmal wiederholen, dann wie Schritt 3 (warten auf GPIO26, höchstens 60 s).

## 6. Prüfung

- Rahmen, CRC, Sitzung, Ablehnung alter IDs und Dach-Zeitüberschreitung haben Tests auf beiden Seiten: Python (`tests/test_link.py`) und Firmware (`xiao/test/test_link.c`).
- Prüfstand: Pi während laufender Kühlbox-Regelung ausschalten → Regelung läuft weiter (S14); Pi während einer Dachfahrt ausschalten → Dach stoppt innert 300 ms (S3).
