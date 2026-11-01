# Camping Unit – Arbeitsregeln

Ersatz des Bedienteils E153 im VW T6 California (MJ 2018). Firmware für einen
ESP32-P4 (Viewe ESP32-P4-Pi-VIEWE) in C mit ESP-IDF, Oberfläche in LVGL 9.

## Quellen der Wahrheit
- docs/SOFTWARE_SPEC.md: Verhalten, Sicherheitsregeln, Meilensteine
- docs/HARDWARE.md: Kanalbelegung, Stückliste, offene Punkte
- docs/E153_Pin_Check_Nr133.md: VW-Pins und Aderfarben
- profiles/t6-california-a8l-2018.json: Pinbelegung und Kalibrierwerte
Widersprechen sich Code und diese Dokumente, gelten die Dokumente. Weicht etwas ab,
in docs/ nachführen und im Commit erwähnen.

## Sicherheit
- Regeln S1–S11 (SOFTWARE_SPEC Kapitel 6) haben Vorrang vor allem anderen.
- Jede S-Regel hat einen Test in tests/safety/, benannt nach der Nummer.
- Nur der io-Task schreibt Ausgänge. UI und Webui senden Befehle über die Queue.
- Nie einen Wert erfinden. Fehlt ein Messwert oder eine Kalibrierung:
  Qualität "uncalibrated" oder "stale", Anzeige leer oder grau.
- Nie auf Victron-Topics W/... publizieren.

## Code
- C11. Logik (signals, modules, safety, profile) ohne ESP-IDF und ohne FreeRTOS,
  Uhr als Parameter, damit sie auf dem Host testbar ist.
- Hardware nur hinter Schnittstellen (hal_io_t, display_t, touch_t).
  Das Display ist noch nicht bestätigt; Rückfall Newhaven muss ohne Umbau
  der Logik möglich sein.
- Keine dynamische Speicherverwaltung im Regelpfad nach dem Start.
- Abhängigkeiten über idf_component.yml mit gepinnten Versionen.

## Oberfläche
- Vorlage ui/reference/Camping_Unit_Oberflaeche_v2.html. Layout, Farben,
  Navigation und Texte übernehmen.
- Texte nur aus ui/i18n/de.json, Schweizer Rechtschreibung (ss statt ß).

## Ablauf
- make test vor jedem Commit. Kleine Commits mit klarer Nachricht.
- Gerätecode (Display, Touch, Sequent, MQTT) erst, wenn Daniel meldet,
  dass die Hardware auf dem Tisch liegt.
- Offene Hardware-Fragen nicht raten. Als TODO mit Verweis auf die
  O-Nummer aus HARDWARE.md markieren.
- Am Ende jedes Meilensteins: kurzer Bericht, was fertig ist, was offen ist,
  welche Annahmen getroffen wurden.
