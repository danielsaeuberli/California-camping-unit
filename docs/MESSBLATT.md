# Messblatt Fahrzeug (zum Ausfüllen)

Messaufträge, **keine durchgeführten Messungen**. Werte erst nach der Messung eintragen, dann ins Profil übernehmen (`TODO_MEASURE` ersetzen) und im Commit nennen.
Widerstände am elektrisch getrennten Bauteil messen. Spannungen immer mit Bezugspunkt angeben. Bei Strömen Messpunkt und Aufbau nennen. Wo möglich Rohaufzeichnung (Logikanalysator, Oszilloskop) ablegen unter `docs/measurements/<datum>/`.

Kopf je Messung: Datum · Fahrzeug (FIN-Ende, MJ, PR-Codes) · Messgerät · Umgebungstemperatur · Original-E153 angeschlossen ja/nein.

## 1. Kühlbox (siehe `docs/KUEHLBOX.md`)

| Messpunkt | Zustand / Messung | Zu protokollieren | Wert | Datum |
| --- | --- | --- | --- | --- |
| T32b/10 | Kühlbox aus | Spannung gegen Masse T32b/31, Bezugspunkt | | |
| T32b/10 | ein, ohne Kühlbedarf | Spannung, Bezugspunkt | | |
| T32b/10 | aktive Kühlung | Spannung, Steuerstrom (Messpunkt, Aufbau) | | |
| T32b/10 | Abschalten | Schaltübergang, ggf. Schaltbild/Polung | | |
| T32b/9 | Normalbetrieb | High-/Low-Pegel, Referenz | | |
| T32b/9 | Start, Stopp | Pulsbreiten, Pausen, Wiederholintervall; Rohaufzeichnung | | |
| T32b/9 | beobachtete Fehler | Pulsfolge roh; Anzeige E153 | | |
| G817 an T32b/7–8 (getrennt) | 0 °C | Widerstand, Referenztemperatur, Stabilisierungszeit, Messgerät | | |
| G817 | 5 °C | dito | | |
| G817 | Raumtemperatur (gemessen) | dito | | |
| G817 | weitere Punkte nach Bedarf | dito | | |
| Kühlraum und G817 | mehrere Sollwerte am E153, volle Ein-/Ausschaltzyklen | Sollwert, Temperatur beim Ein- und beim Ausschalten, Zeiten, Umgebung | | |
| Sensorfehler (nur wenn gezielt geprüft) | Fühler unterbrochen | Reaktion E153, Reaktion Kompressor, Anzeigefehler, D-Signal; **keine Code-6-Erwartung vorgeben** | | |
| Typenschilder | Kühlbox, Kompressor, Elektronik | Fotos, Teilenummern, Revisionen | | |

## 2. Heizung, Sollwert T2p/1–T2p/2

Am **getrennten, E153-seitigen** T2p-Anschluss messen (E153 eingeschaltet, Stufe am Original einstellen). Regelrichtung festhalten (wird es mit höherer Stufe wärmer?). Einstellung Eberspächer/Webasto am Original notieren.

| Stufe | Quellenwert (Forum) | frühere Schätzung | gemessen Ω | Datum |
| --- | ---: | ---: | ---: | --- |
| 1 | 333 | – | | |
| 2 | – | ca. 439 | | |
| 3 | – | ca. 558 | | |
| 4 | – | ca. 690 | | |
| 5 | 839 | – | | |
| 6 | – | ca. 1008 | | |
| 7 | – | ca. 1201 | | |
| 8 | – | ca. 1424 | | |
| 9 | – | ca. 1685 | | |
| 10 | 1993 | – | | |

Quelle der Werte 1, 5, 10: Tristan, Pekaway-Forum, Abschnitt «Eberspächer», https://forum.pekaway.de/t/webasto-und-der-w-bus-erklart-und-viele-gerate-implementieren/1897 – kein Nachweis für dieses Fahrzeug. Die Schätzungen sind nicht belegt und werden nicht verwendet.

| Weitere Heizungsmessung | Zu protokollieren | Wert |
| --- | --- | --- |
| T6ap/2 und T6ap/3: welcher ist S+, welcher die Betriebsanzeige | Pegel bei aus, Start, Heizen, Nachlauf | |
| Betriebsanzeige Zeitverlauf | Start bis «an», Nachlauf bis «aus» | |
| Verbauter Heizungstyp | Typenschild Airtronic/Webasto | |

## 3. Dach, Fahrzeug, Tanks, Netzwerk (aus HARDWARE.md Kapitel 7)

| Messpunkt | Zu protokollieren | Wert |
| --- | --- | --- |
| T32b/11, /12 | Pegel und Polarität beim Öffnen/Schliessen am Original | |
| T32b/14, /15 | Wahrheitstabelle in allen vier Dachzuständen (zu, öffnet, offen, schliesst) | |
| Pumpen-Nachlauf | Zeit vom Ende des Befehls bis Pumpe still | |
| T32b/3 | Geschwindigkeitssignal: Amplitude, Pulse je km bzw. Frequenz bei bekannter Geschwindigkeit | |
| T32b/28, /29, /30 | 86s, D+ (Motor aus/an), Zündung 15a | |
| T32b/27, T6ap/4 | 58d Wellenform (PWM?) | |
| G120 | Widerstand je Stufe (leer, 1/3, 2/3, voll) | |
| G126 | Widerstand leer/voll | |
| G249 | Kennlinie (mind. 3 Punkte) | |
| AI8 / T32b/32 | Batteriespannung, Spannungsabfall über SH21 | |
| Cerbo | MQTT über dessen Access Point (O10) | |
