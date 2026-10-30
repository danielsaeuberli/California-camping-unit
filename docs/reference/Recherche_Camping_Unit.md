# Recherche Camping Unit: Online-Quellen

Stand 27. Oktober 2026 | Fahrzeug: VW T6 California 2.0 TDI 4Motion, MJ 2018 | Ergänzt *E153_Pin_Check_Nr133.md*

Forenbeiträge sind Erfahrungswerte von Nutzern, keine Herstellerangaben. Sie sind entsprechend gekennzeichnet.

## 1. Pinbelegung E153: unabhängig bestätigt

Im VW California Club hat ein Nutzer (vbk1975) 2021 eine Belegung aus den T6-Schaltplänen gepostet. Sie deckt sich Pin für Pin mit unserem Abgleich von Nr. 133: T6ap 1–6, T32b 3, 7–12, 14–22, 25–32 und T2p 1–2. Damit sind zwei Lesarten derselben VW-Quelle konsistent. Eine Messung am Fahrzeug ersetzt das nicht.

Derselbe Thread zeigt: Ein Ersatz von E153 wurde dort angedacht (Bluetooth, eigene MCU), aber nicht fertiggestellt. Ein Raspberry Pi wurde dort wegen des Stromverbrauchs als zu durstig eingeschätzt.

## 2. Was E153 selbst überwacht (Fehlercodes)

Die versteckten Diagnose-Codes zeigen, welche Anschlüsse E153 als Ein- und Ausgänge behandelt:

| Code | Bedeutung | Folgerung für die Belegung |
| --- | --- | --- |
| 1100 / 1110 | Kurzschluss Dach-Ausgang «auf» / «zu» | Pins 11/12 sind Ausgänge von E153 mit Stromüberwachung |
| 1101 / 1111 | Sicherung Dach-Ausgang | ebenso |
| 2100 / 2101 | Kurzschluss / Sicherung Heizungsausgang ein/aus | S+ (T6ap/3) ist ein überwachter Ausgang |
| 2001 | Unterbrechung Heizungseingang 30 | Klemme 30 über T6ap/1 wird geprüft |
| 3100 / 3101 | Kurzschluss / Unterbrechung Kühlbox-Ausgang | Pin 10 ist ein überwachter Ausgang |
| 3001 | Eingangsfehler Kühlbox J698 | Pin 9 (Klemme D) wird als Fehlersignal gelesen |
| 3010 / 3011 | Kurzschluss / Unterbrechung Kühlschrankfühler | G817 an Pins 7/8 wird auf offen und kurz geprüft |
| 4000/4001, 5000/5001 | Kurzschluss nach Masse / Plus, Wasserstandsgeber | Tankgeber sind Widerstandsgeber mit Plausibilitätsprüfung |
| 6000 / 6001 | dito, Aussentemperatur | G249 ebenso |
| 7001 | Unterbrechung Eingang D+ | E153 liest ein D+-Signal (Motor läuft). Hypothese: das ist Pin 29 von J519. Am Fahrzeug prüfen. |

Für den Ersatz heisst das: Die Ausgänge 10, 11, 12 und T6ap/3 liefert E153 vermutlich selbst als Halbleiter-Ausgänge mit Strommessung. Ein Relais-Kontakt ersetzt das, solange Pegel und Strom passen. Polarität (schaltet nach Plus oder nach Masse) weiterhin messen.

## 3. Dach (J768)

- Forum: E153 ist nur die Bedienoberfläche. J768 steuert die Pumpe und liest vier Geber an den Hydraulikzylindern.
- Die vier Leitungen E153–J768 werden dort als zwei Anfragen (auf/zu) und zwei Rückmeldungen gedeutet. Ein serieller Bus wurde vermutet, ist aber nicht belegt. Die Fehlercodes 1100–1111 sprechen eher für einfache geschaltete Ausgänge.
- Geber: vermutlich Hall-Schalter mit zwei Leitern (Typ HAL566 o. ä.), etwa 3 mA bzw. 14 mA je nach Zylinderstellung. Annahme eines Nutzers aus Messungen.
- Bekannte Fehlerbilder: Dach fährt nicht oder Pumpe läuft nach dem Absenken weiter, wenn J768 die Endlage nicht erkennt.

## 4. Standheizung (J604, Eberspächer Airtronic)

Belegung des Steckers T10ba an J604 laut zwei Threads im TX-Board. Die Farben decken sich mit Nr. 133/6:

| T10ba | Farbe | Funktion (Forum) | Kommt von E153 |
| --- | --- | --- | --- |
| 9 | ge | S+ Einschaltsignal | T6ap/3 |
| 2 | rs | zum Dachbedienteil, Funktion offen | T6ap/2 |
| 7 | gr/rt | zum Dachbedienteil, Funktion offen | T2p/1 |
| 10 | br/sw | zum Dachbedienteil, Funktion offen | T2p/2 |
| 3 | bl/ws | Diagnose | – |
| 4 | rt | +12 V von Zweitbatterie (25 A) | – |
| 8 | br | Masse | – |
| 5 / 6 | br / gn/br | Dosierpumpe | – |

- **Damit ist T6ap/3 = S+ bestätigt.** T6ap/2 bleibt die Rückmeldung (Betriebsanzeige). T2p/1 und /2 sind der Sollwertgeber.
- Eberspächer: Sollwert einstellbar zwischen **+10 °C und +30 °C**. Nach dem Ausschalten läuft das Gebläse rund **4 Minuten** nach, die Glühkerze etwa 40 s zum Freibrennen. Die Speisung darf in dieser Zeit nicht getrennt werden.
- **Widerstandswerte des Sollwertgebers sind online nicht zu finden.** Die 1750–2179 Ω aus dem Workbook bleiben unbestätigt. Messen.

## 5. Kühlbox (J698) und Steuergerät J1029

- Laut Forum ist in der California-Kühlbox ein Danfoss/Secop **BD35F** (ersetzt Frigomatic 1E50960). Typenschild prüfen.
- Speed-Widerstand R1 laut Secop-Datenblatt (Elektronik 101N0242/0510/0680; bei AEO-Varianten andere Werte):

| R1 | Drehzahl |
| --- | --- |
| 0 Ω | 2000 U/min |
| 277 Ω | 2500 U/min |
| 692 Ω | 3000 U/min |
| 1523 Ω | 3500 U/min |

- LED-Fehlercodes an Klemme D laut Secop: 1 Batterieschutz, 2 zu viele Startversuche / Lüfter-Überstrom, 3 Motorstart (blockiert), 4 Mindestdrehzahl nicht gehalten, 5 Übertemperatur Elektronik, 6 Thermostatfehler. Die Hypothese im Workbook (1–5 Blinkimpulse, Rahmen von 4 s) passt dazu. Code 6 ergänzen.
- J1029 (7H7 068 455 E) sitzt zwischen E153 Pin 10 und den Klemmen C/P/T der Kühlbox. Vermutlich enthält es den Drehzahlwiderstand. Mit J1029 an Ort reicht ein Relais auf Pin 10.

## 6. Wassertanks

- Frischwasser G120: Schwimmer mit 3 Reed-Kontakten und Widerstandsnetz, also **4 Stufen** (leer, 1, 2, 3). Ersatzteil 7E7 068 257 B (Edelstahlrohr; alte Kunststoffrohre reissen und verfälschen den Widerstand).
- Abwasser G126: laut Nutzern praktisch nur **leer / voll**.
- Widerstandswerte je Stufe sind online nicht dokumentiert. Messen.

## Was danach noch gemessen werden muss

1. Widerstand über T2p am Original-Panel bei Minimum und Maximum (+10 / +30 °C)
2. Pegel und Polarität an Pins 10, 11, 12 und T6ap/3 im Betrieb
3. Signal an Pin 29 bei laufendem Motor (D+-Hypothese)
4. Widerstand G120 je Stufe, G126 leer/voll, Kennlinie G817 und G249
5. Typenschild Kompressor und Elektronik in der Kühlbox

## Quellen

- VW California Club, «Redesigning control panel»: https://mail.vwcaliforniaclub.com/goto/post?id=517624
- VW California Club, «Control Panel Diagnostics»: https://mail.vwcaliforniaclub.com/threads/control-panel-diagnostics.32662/latest
- VW California Club, «Error Codes for the VW Cali»: https://vwcaliforniaclub.com/goto/post?id=230437
- VW California Club, «Roof code 1000»: https://mail.vwcaliforniaclub.com/goto/post?id=557757
- VW California Club, «Fresh water level sensor for T5»: https://mail.vwcaliforniaclub.com/goto/post?id=571273
- VW California Club, «Water Sensors»: https://mail.vwcaliforniaclub.com/goto/post?id=204454
- TX-Board, T10ba-Belegung Airtronic: https://tx-board.de/threads/hilfe-bei-kabelbelegung-von-t10ba-stecker-von-eberspaecher-airtronic-m-d3-plus-mit-easystart-timer.116767/
- TX-Board, Pinbelegung Airtronic M D3 Plus: https://tx-board.de/threads/schaltplan-kabelbelegung-pinbelegung-kabelbaum-eberspaecher-airtronic-m-d3-plus.55226/
- Eberspächer Airtronic Technische Beschreibung: https://eberspacherdirect.com/wp-content/uploads/2020/09/Eberspacher-Airtronic-D2-Manual-Technical-Description.pdf
- Secop BD35F Datenblatt: https://selector.secop.com/fileadmin/user_upload/SEPS/datasheets/en/bd35f_101z0204_r134a_12-24vdc_01-2024_desd100p722.pdf
- minitools, Reparatur Bedienteil T5/T6 California: https://www.minitools.com/volkswagen-t5-t6-california-camper-control-panel-repair/
