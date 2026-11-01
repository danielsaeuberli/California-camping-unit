# Camping Unit – Arbeitsregeln

Ersatz des Bedienteils E153 im VW T6 California (MJ 2018). Firmware für einen
ESP32-P4 (Viewe ESP32-P4-Pi-VIEWE) in C mit ESP-IDF, Oberfläche in LVGL 9.

## Quellen der Wahrheit
- docs/SOFTWARE_SPEC.md: Verhalten, Sicherheitsregeln, Meilensteine
- docs/HARDWARE.md: Kanalbelegung, Stückliste, offene Punkte
- docs/E153_Pin_Check_Nr133.md: VW-Pins und Aderfarben
- docs/KUEHLBOX.md: Evidenz Kühlbox (Quelle/Messung/Hypothese); docs/MESSBLATT.md: Messaufträge
- profiles/t6-california-a8l-2018.json: Pinbelegung und Kalibrierwerte
Widersprechen sich Code und diese Dokumente, gelten die Dokumente. Weicht etwas ab,
in docs/ nachführen und im Commit erwähnen.

## Sicherheit
- Regeln S1–S11 (SOFTWARE_SPEC Kapitel 6) haben Vorrang vor allem anderen.
- Jede S-Regel hat einen Test in tests/safety/, benannt nach der Nummer.
- Nur der io-Task schreibt Ausgänge. UI und Webui senden Befehle über die Queue.
- Nie einen Wert erfinden. Fehlt ein Messwert oder eine Kalibrierung:
  Qualität "uncalibrated" oder "stale", Anzeige leer oder grau.
- Victron: lesen; schreiben nur vebus/<inst>/Mode (Modi aus victron.modes) und
  vebus/<inst>/Ac/In/1/CurrentLimit, nur auf Bedienung, nur mit victron.control: true
  im Profil (SPEC 5.1, Entscheid Daniel 1.11.2026). Keine anderen W/...-Topics.
- Ein Ausgang schaltet nur mit Freigabe (release.*) und bestätigter Ausgangsart
  (outputs.*.drive); Entwurfsannahmen wie «+12 V» nie als Geräteeigenschaft verwenden.
- Simulationswerte nur in profiles/sim/, in der Oberfläche als «Simulation» gekennzeichnet.

## Code
- C11. Logik (signals, modules, safety, profile) ohne ESP-IDF und ohne FreeRTOS,
  Uhr als Parameter, damit sie auf dem Host testbar ist.
- Hardware nur hinter Schnittstellen (hal_io_t, display_t, touch_t).
  Das Display ist noch nicht bestätigt; Rückfall Newhaven muss ohne Umbau
  der Logik möglich sein.
- Keine dynamische Speicherverwaltung im Regelpfad nach dem Start.
- Abhängigkeiten über idf_component.yml mit gepinnten Versionen.

## Oberfläche
- Vorlage ist die aktuelle Weboberfläche aus v1: ui/reference/web_v1/ (Entscheid
  Daniel 1.11.2026). Layout, Farben, Navigation und Texte übernehmen.
  ui/reference/Camping_Unit_Oberflaeche_v2.html ist der ältere Prototyp.
- Texte nur aus ui/i18n/de.json und ui/i18n/en.json (Basis: Sprachdateien der
  v1-Weboberfläche). Jeder neue Text in beide Dateien mit gleichen Platzhaltern; Deutsch in
  Schweizer Rechtschreibung (ss statt ß). tools/i18n_gen.py erzeugt die C-Tabellen und prüft
  Schlüssel, Platzhalter und dass jedes Zeichen in den Schriften (tools/gen_fonts.sh) ist.

## Ablauf
- make test vor jedem Commit. Kleine Commits mit klarer Nachricht.
- Gerätecode (Display, Touch, Sequent, MQTT) erst, wenn Daniel meldet,
  dass die Hardware auf dem Tisch liegt.
- Offene Hardware-Fragen nicht raten. Als TODO mit Verweis auf die
  O-Nummer aus HARDWARE.md markieren.
- Am Ende jedes Meilensteins: kurzer Bericht, was fertig ist, was offen ist,
  welche Annahmen getroffen wurden.
