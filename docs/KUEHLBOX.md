# Kühlbox – Evidenz, Stand und offene Punkte

Stand 1. November 2026. Grundlage: Übergabe «Kühlbox und Heizungs-Sollwert» (Daniel, 1.11.2026), Bestand im Repository (`docs/E153_Pin_Check_Nr133.md`, `docs/reference/Recherche_*.md`, `docs/HARDWARE.md`, `docs/SOFTWARE_SPEC.md`). Keine neue Prüfung am Fahrzeug, keine neue Prüfung der Originalunterlagen.

**Evidenzklassen:** **Q** = Quelle (Stromlaufplan, Datenblatt, Forum; mit Fundstelle) · **M** = am Fahrzeug gemessen · **H** = Hypothese/Entwurfsannahme · **U** = unbekannt.
Für die Kühlbox gibt es bis heute **keinen einzigen M-Wert**.

## 1. Anschlüsse und Bauteile

| Anschluss / Bauteil | Aussage | Klasse | Fundstelle, Datum | Offen |
| --- | --- | --- | --- | --- |
| T32b/7, T32b/8 | G817 Kühlbox-Temperaturfühler (/7 ws/br → G817 Pin 2, /8 gn/br → G817 Pin 1) | Q | Stromlaufplan Nr. 133, Blatt 9 (`E153_Pin_Check_Nr133.md`, Zeilen 07/08), Abschrift vom 30.10.2026 | Fühlertyp, Kennlinie, Widerstand je Temperatur |
| G817 Widerstand | «knapp über 500 Ω am Stecker», Temperatur nicht angegeben | H (Forenangabe, keine eigene Messung) | VW California Club, zitiert in `reference/Recherche_Camping_Unit.md` | als Kennlinie unbrauchbar; nicht verwenden |
| G817 Ersatzteil | Dometic 296.9711.01 | H | dito | Datenblatt dieses Teils suchen; ähnliche Sensoren gelten nicht als Beleg |
| T32b/9 | Diagnoseleitung, Klemme D der Kühlbox-Elektronik (J698) | Q | Nr. 133/9 (`E153_Pin_Check_Nr133.md`, Zeile 09) | Pegel, Ausgangstyp, Bezugspotential, Belastbarkeit, Codierung |
| T32b/9 «low-aktiv» | Diagnose-Impulse ziehen nach Masse | H | Arbeitshypothese aus Secop-Standard | am Fahrzeug messen |
| T32b/9 Codes | 1–5 Impulse, ca. 250 ms, ca. 4 s Pause; Code 6 bei Unterbrechung G817 | H | generische Secop-Angabe (`reference/Recherche_VW_Schaltplaene_2026-10-30.md`); Code 6 unbelegt | nicht als Fahrzeugprotokoll verwenden |
| T32b/10 | Kühlanforderung zu J1029 (Teilenummer 7H7 068 455 E), Klemme K4; ein Widerstand überbrückt zur Minusleitung von J698 | Q | Nr. 133/9 (`E153_Pin_Check_Nr133.md`, Zeile 10) | Polarität, Spannung, Strom, Ruhezustand |
| T32b/10 «+12 V» | E153 schaltet +12 V | **H** (Entwurfsannahme) | frühere HARDWARE-Tabellen | **nicht** allein deshalb einen 12-V-Treiber aktivieren |
| E153 Ausgang Pin 10 | überwachter Ausgang (Fehlercodes 3100/3101 Kurzschluss/Unterbrechung) | Q | `reference/Recherche_Camping_Unit.md`, Fehlercodetabelle | ob J1029 einen Relaiskontakt ohne Fehlermeldung akzeptiert: offen |
| E153 Thermostat | wertet G817 aus (Fehlercodes 3010/3011 Fühler kurz/offen) und schaltet die Anforderung | Q (Funktion), U (Werte) | dito | Sollwertbereich, Hysterese, Schaltzeiten |
| Kompressor / Elektronik | Secop/Danfoss BD35F | H | Forum, zitiert in `reference/Recherche_Camping_Unit.md` | Typenschilder Kompressor **und** Elektronik |
| Kontakt C–T als Anforderung | aus der möglichen Secop-Elektronik-Architektur | H | Secop-Datenblatt (allgemein) | erst am identifizierten Elektroniktyp prüfen; **nicht** mit T32b/10 gleichsetzen |
| J698 / J1029 / J472 | Bezeichnungen aus Plan und früheren Notizen | Q (J698, J1029 im Plan Nr. 133), U (J472) | J472 kommt im Repository nicht vor | an Plan und Typenschild der verbauten Variante auflösen; nicht austauschbar behandeln |
| Kompressorversorgung | separat, nicht über die Camping-Unit | H | frühere Notizen | Versorgungsweg und Absicherung am Plan bestätigen |
| Temperaturwahl über Widerstandsleiter | wie bei der Heizung | U | nicht gefunden | – |

## 2. Was die Software daraus macht (Stand v2)

- **Profil** (`profiles/t6-california-a8l-2018.json`): `curves.g817`, `analog.fridge_temp.pullup_ohm`, Sollbereich und Hysterese (`fridge.target_*`, `on_above_c`, `off_below_c`), `fridge.diag_polarity` und die Ausgangsart `outputs.fridge.drive` sind `TODO_MEASURE`. `fridge.diag_decoder` ist `none`. Keine Standard-NTC-Kurve, kein Beta-Wert.
- **Getrennte Teile** (`firmware/components`): Temperaturerfassung (Rohspannung AI1, Ohm, Temperatur mit Qualität und Zeitstempel), Thermostat (`fridge_step`), Kühlanforderung als Ausgang (`control.c`, nur mit bestätigter Ausgangsart und Freigabe), Diagnose (Rohaufzeichnung der Pegelwechsel immer; Decoder nur mit gewähltem Profil).
- **Zustände:** «Thermostat nicht kalibriert», «Fühler gestört», «Kompressorschutz: Pause/Mindestlaufzeit», «Temperatur erreicht», «Kühlung angefordert». Anforderung (Thermostat) und angesteuerter Ausgang K3 werden getrennt geführt; der **Kompressorstatus ist immer «unbekannt»**, weil es keine Rückmeldung gibt.
- **Ohne Kennlinie:** keine Temperatur, nur die Rohspannung. Veraltete oder fehlende Werte werden nie als aktuelle Temperatur gezeigt.
- **Sensorfehler, Datenverlust:** Thermostat fordert nichts mehr an (nach Ablauf der S6-Mindestlaufzeit), Meldung «Kühlbox-Fühler ohne gültigen Wert».
- **Neustart:** Anforderung aus, dann gilt die S6-Mindestpause. Der Wunsch «Kühlbox ein» wird noch nicht gespeichert (Einstellungen in LittleFS, späterer Meilenstein).
- **Befehle:** nur über die interne Queue mit steigender Nummer; es gibt in v2 keine MQTT-Befehle. Victron wird nur gelesen (SPEC 5).
- **Simulation:** `profiles/sim/sim-overrides.json` setzt erfundene, als Simulation gekennzeichnete Werte (Kennlinie, Hysterese 1,5/1,0 °C, Sollbereich 1–10 °C, Ausgangsart, Decoder `secop_hypothesis`). Die Oberfläche zeigt dann den Hinweis «Simulation». Diese Werte sind keine Fahrzeugdaten.
- **S6** (Mindestlaufzeit 2 min, Mindestpause 3 min) ist eine Vorgabe der SPEC zum Kompressorschutz, keine Messung; mit dem Datenblatt der identifizierten Elektronik abgleichen.

## 3. TODO (Übergabe P0–P4)

| Nr. | Aufgabe | Stand |
| --- | --- | --- |
| P0.1 | Bestand nach G817, T32b, J698, J1029, J472, BD35F, Kühlbox durchsuchen | erledigt (Tabelle 1) |
| P0.2 | Jede elektrische Aussage mit Q/M/H/U kennzeichnen | erledigt für die Kühlbox (Tabelle 1); HARDWARE.md 4 verweist hierher |
| P0.3 | +12-V-Ansteuerung, G817-Kurven, Diagnosecodes aus der aktiven Konfiguration nehmen | erledigt: `drive`, Kurve, Polarität `TODO_MEASURE`, Decoder `none` |
| P0.4 | Fehlende Unterlagen auflisten | erledigt (Abschnitt 4) |
| P1.1 | Passenden VW-Stromlaufplan prüfen, T32b/7–10 bis zum Ziel verfolgen | offen: Plan Nr. 133 ist abgeschrieben; Ausgabe «ab Juni 2015» auf MJ 2018 abgleichen |
| P1.2 | Typenschilder Kühlbox, Kompressor, Elektronik | **offen, Hardware-Blocker** (Fotos von Daniel) |
| P1.3 | Herstellerunterlagen der identifizierten Elektronik, C/T/D abgleichen | offen, hängt an P1.2. Kandidat (H): Secop BD35F, Datenblatt-Link in `reference/Recherche_Camping_Unit.md` |
| P1.4 | G817-Kennlinie aus Original-/Herstellerdaten | offen; Ersatzteilnummer Dometic 296.9711.01 als Suchansatz |
| P1.5 | Ausgangsstufe festlegen (Relais, potentialfreier Kontakt, Open-Drain …) | offen, erst nach P1.1–P1.3 und Messung T32b/10 |
| P2 | Messblatt | erledigt: `docs/MESSBLATT.md`, Abschnitt Kühlbox |
| P3.1–P3.11 | Software mit expliziten Schnittstellen | erledigt in v2 (Abschnitt 2), Tests `tests/unit/test_fridge.c`, `tests/safety/test_s6_fridge_min_times.c`, Oberfläche Kühlbox-Seite |
| P4.1 | Checks: Schwellen, Datenverlust, Sensorfehler, Neustart | erledigt mit synthetischen Daten (`make test`) |
| P4.2 | Unbestätigte Profile steuern keine Ausgänge; Anforderung ≠ Ist-Status | erledigt (`test_fridge.c`) |
| P4.3 | Mit Rohaufzeichnungen vom Fahrzeug prüfen | offen, sobald Messungen vorliegen |

## 4. Fehlende Unterlagen

- Fotos/Abschriften der Typenschilder: Kühlbox, Kompressor, Kompressorelektronik (Teilenummer, Revision).
- Datenblatt der tatsächlich verbauten Elektronik (Klemmen C, T, D, P; Diagnoseprotokoll).
- Datenblatt oder Ersatzteildaten G817 (Kennlinie), z. B. zu Dometic 296.9711.01.
- Stromlaufplan der Ausführung MJ 2018 mit Ausstattung A8L/A8R (die vorhandene Abschrift Nr. 133 und das genannte `K0058981600-Grundausstattung__ab_Juni_2015.pdf` müssen auf 2018 abgeglichen werden).
- Unterlagen zu J1029 (7H7 068 455 E): Eingangsschaltung K4, Überwachung.
- In früheren Gesprächen genannt, im Repository nicht vorhanden: `Camping_Unit_modulare_Stueckliste.xlsx`, `Cali_Pinmatrix_und_Messplan_v1.md`, `Camping_Unit_Cali_Gesamtplanung_v1.md`, `K0058981600-Grundausstattung__ab_Juni_2015.pdf`.
