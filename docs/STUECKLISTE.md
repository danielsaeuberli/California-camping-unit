# Cali Camping Unit – Stückliste und Bezugsquellen

Stand 2. November 2026. Positionen, Mengen und Budgetpreise aus [HARDWARE.md](HARDWARE.md) Kapitel 3 (Version 2.1); bei Abweichungen gilt HARDWARE.md. Preise sind Budgetwerte in CHF, keine Offerten. Shop-Links am 2. November 2026 recherchiert; Preise und Lagerstand können abweichen. «(Suche)» führt zu einer Suche beim Händler, sonst zu einer Produktseite.

Status: **Bestellen** = Modell steht fest. **Erst klären** = in HARDWARE.md «Bestätigen», «Anfragen», «Prüfen», «Abstimmen» oder «Geplant»; offene Frage (O-Nummer) vor der Bestellung klären. **Nicht bestellen** = Wert hängt von einer fehlenden Messung ab.

| Pos | Gruppe | Bauteil | Modell | Menge | CHF | Status | Wo kaufen | Hinweis |
| --- | --- | --- | --- | ---: | ---: | --- | --- | --- |
| 1 | Rechner | ESP32-P4-Board mit C6, Ethernet, 2-Lane-DSI | Viewe ESP32-P4-Pi-VIEWE | 1 | 18.00 | Bestellen | [Viewe (Hersteller)](https://viewedisplay.com/)<br>[Doku und Schaltpläne (Gitee)](https://gitee.com/VIEWESMART/ESP32-P4-Pi)<br>[AliExpress](https://www.aliexpress.com/w/wholesale-ESP32-P4-Pi-VIEWE.html) (Suche) | 17.99 USD im Viewe-Shop, rund 33 USD inkl. Versand auf AliExpress (Stand März 2026) |
| 3 | Rechner | microSD 32 GB High Endurance | A1 | 1 | 18.00 | Erst klären | [Galaxus](https://www.galaxus.ch/de/search?q=microSD%2032GB%20High%20Endurance) (Suche) | Kartenslot am Board prüfen |
| 4 | Rechner | RTC-Batterie oder -Akku | passend zum Board | 1 | 6.00 | Erst klären | – | RTC am Board prüfen; Typ erst danach |
| 5 | Anzeige | 6,8" 480×1280 IPS, MIPI DSI, CTP über I²C | Viewe, Bestellnummer angefragt | 1 | 45.00 | Erst klären | [Viewe anfragen](https://viewedisplay.com/) | O15. Nicht die HDMI-Version UEDX48128068L-HMD-H, die bei Händlern gelistet ist |
| 6 | Anzeige | FPC- oder Zwischenkabel Board ↔ Display | Viewe | 1 | 5.00 | Erst klären | [Viewe anfragen](https://viewedisplay.com/) | O15; Länge nach Einbau |
| 11 | I/O | Home Automation HAT | Sequent SM-H-001 | 1 | 80.00 | Erst klären | [Sequent Microsystems](https://sequentmicrosystems.com/products/home-automation-raspberry-pi-hat) | Geplant; Montage abgesetzt (O17, O20) |
| 12 | Strom | DC/DC 12 → 5 V, 3 A, kleiner Ruhestrom | Pololu D36V28F5 | 1 | 30.00 | Erst klären | [Pololu #3782](https://www.pololu.com/product/3782)<br>[EXP Tech (DE)](https://exp-tech.de/en/products/5v-32a-step-down-voltage-regulator-d36v28f5)<br>[Opencircuit (NL)](https://opencircuit.shop/Product/5V-3.2A-Step-Down-Voltage-Regulator-D36V28F5) | Vorschlag. 5,3–50 V ein, 3,2 A, Ruhestrom max. 3 mA laut Pololu; Wirkungsgrad bei kleiner Last prüfen (O19) |
| 13 | I/O | 16 LV-Digitaleingänge (3–24 V) | Sequent SM-I-009 | 1 | 60.00 | Erst klären | [Sequent Microsystems](https://sequentmicrosystems.com/products/sixteen-lv-digital-inputs-card-for-raspberry-pi) | Geplant; Adresse 0x27, Interrupt-Pin (O17) |
| 14 | Heizung | 4-fach-Relaismodul, getrennte COM/NC/NO | Purecrea 420053 | 1 | 12.90 | Erst klären | [Bastelgarage](https://www.bastelgarage.ch/4-channel-relay-module)<br>[Digitec](https://www.digitec.ch/en/s1/product/purecrea-4-channel-relay-module-development-boards-kits-34239687) | CHF 12.90 bei Bastelgarage. Schaltet es mit 3,3-V-Logik? (O18) |
| 15 | Bedienung | Summer 5 V + Transistor | Piezo aktiv | 1 | 4.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=Piezo%20Summer%20aktiv%205V) (Suche)<br>[Bastelgarage](https://www.bastelgarage.ch/search?search=buzzer) (Suche) |  |
| 16 | Heizung | Präzisionswiderstand Basis der Sollwertleiter | Metallfilm 0,1 % | 1 | 4.00 | **Nicht bestellen** | – | Wert erst nach Messung T2p je Stufe (O23). 1750 Ω ist ein unvalidierter Entwurf |
| 17 | Heizung | Präzisionswiderstände Segmente H1–H4 | Metallfilm 0,1 % | 1 | 8.00 | **Nicht bestellen** | – | Dito; 27,4 / 54,9 / 110 / 237 Ω gehören zum unvalidierten Entwurf |
| 18 | Fahrzeug | Gegenstecker T32b | VW, Codierung am E153 ablesen | 1 | 35.00 | Erst klären | [VW-Teilehandel](https://www.ebay.de/sch/i.html?_nkw=T32b%20Stecker%20California) (Suche) | Teilenummer nach Gehäusefoto |
| 19 | Fahrzeug | MQS-Kontakte/Reparaturleitungen | 32 + 6 Reserve | 38 | 83.60 | Bestellen | [eBay: VW Reparaturleitung MQS](https://www.ebay.de/sch/i.html?_nkw=VW%20Reparaturleitung%20MQS) (Suche)<br>[VW-Garage (Teilenummer)](https://www.oemwolf.com/) | Teilenummer zum Stecker passend wählen; 000979009E ist nicht sicher MQS |
| 20 | Fahrzeug | Gegenstecker T6ap komplett | VW | 1 | 18.00 | Erst klären | [VW-Garage](https://www.ebay.de/sch/i.html?_nkw=VW%206%20polig%20Stecker%20Standheizung) (Suche) | Heizung, Zündung |
| 21 | Fahrzeug | Gegenstecker T2p komplett | VW | 1 | 10.00 | Erst klären | [VW-Garage](https://www.ebay.de/sch/i.html?_nkw=VW%202%20polig%20Stecker%20Reparatursatz) (Suche) | Heizungssollwert |
| 22 | Breakout | 8-pol. Panel-Durchführung | Phoenix DFK-MSTB 2,5/8-GF-5,08 | 5 | 110.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=DFK-MSTB%202%2C5%2F8-GF-5%2C08) (Suche)<br>[RS Online](https://de.rs-online.com/web/p/leiterplattensteckverbinder/2205083) | X1–X4 T32b, X5 T6ap + T2p |
| 23 | Breakout | 8-pol. Gegenstecker | Phoenix MSTB 2,5/8-STF-5,08 | 10 | 100.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=MSTB%202%2C5%2F8-STF-5%2C08) (Suche)<br>[element14](https://cn.element14.com/phoenix-contact/mstb-2-5-8-stf-5-08/-8-12awg/dp/2671289) | 5 innen, 5 aussen |
| 24 | Verdrahtung | FLRY-B 0,35 mm², farbig | Meter | 25 | 30.00 | Bestellen | [Conrad](https://www.conrad.ch/de/search.html?search=FLRY-B%200%2C35) (Suche)<br>[Distrelec](https://www.distrelec.ch/de/search?q=FLRY-B%200.35) (Suche) | Signalleitungen |
| 25 | Verdrahtung | FLRY-B 1,0 mm² rot/braun | Meter | 5 | 10.00 | Bestellen | [Conrad](https://www.conrad.ch/de/search.html?search=FLRY-B%201%2C0) (Suche) | Versorgung, Masse |
| 26 | Verdrahtung | Adermarker 01–32 + Schrumpfschlauch | Set | 1 | 22.00 | Bestellen | [Conrad](https://www.conrad.ch/de/search.html?search=Kabelmarkierer%20Ziffern) (Suche) | Beschriftung beidseitig |
| 27 | Verdrahtung | Micro-Fit-Serviceverbinder 12-pol. | Molex Micro-Fit 3.0 | 3 | 36.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=Molex%20Micro-Fit%2012) (Suche) | Module trennbar; Gehäuse, Stiftleiste und Kontakte zusammen bestellen |
| 28 | Strom | ATO-Sicherungshalter + 7,5 A | Inline | 1 | 12.00 | Bestellen | [Conrad](https://www.conrad.ch/de/search.html?search=Flachsicherungshalter%20inline) (Suche) | F1, eigene Zuleitung, nicht über SH21 |
| 29 | Strom | Lastabwurf-TVS | Littelfuse SLD15-018 | 1 | 22.00 | Erst klären | [Littelfuse](https://www.littelfuse.com/products/overvoltage-protection/tvs-diodes/automotive-tvs-diodes/sld/sld15-018)<br>[Digi-Key](https://www.digikey.ch/en/products/detail/littelfuse-inc/SLD15-018/4556561)<br>[Octopart (Preisvergleich)](https://octopart.com/part/littelfuse/SLD15-018) | Bidirektional, 15 V Standoff, P600. Klemmspannung mit dem Wandler (Pos. 12, max. 50 V ein) abstimmen; Lieferzeit teils 12 Wochen |
| 30 | Strom | Verpolschutz 10 A | Ideal-Diode-Modul | 1 | 35.00 | Erst klären | [Pololu Ideal Diode 4–60 V](https://www.pololu.com/search?query=ideal+diode+reverse+voltage+protector) (Suche)<br>[RobotShop (12-A-Version)](https://eu.robotshop.com/products/pololu-ideal-diode-reverse-voltage-protector-4-60v-12a) | Vorschlag: Pololu-Modul 10 A oder 12 A (LM74700-Q1); Wärme bei Dauerstrom prüfen |
| 36 | Service | Panel-USB-C-Verlängerung | mit Kappe | 1 | 18.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=USB-C%20Einbaubuchse%20Panel%20Kappe) (Suche) | Flashen, Service |
| 41 | I/O | I²C-Leitung zu beiden Sequent-Karten | LiYCY 4×0,25 mm² + 2×20-Adapter | 1 | 15.00 | Erst klären | [Conrad](https://www.conrad.ch/de/search.html?search=LiYCY%204x0%2C25) (Suche) | max. ca. 30 cm; Pull-ups 2,2 kΩ (O17) |
| 42 | Netz | Patchkabel Cat 6, flach | Länge nach Einbau | 1 | 8.00 | Bestellen | [Galaxus](https://www.galaxus.ch/de/search?q=Patchkabel%20Cat6%20flach) (Suche) | P4 ↔ Camper-Router |
| 43 | Messung | Messnetzwerk: Pull-ups, Teiler, BAT54S | Metallfilm 0,1 %, Sortiment | 1 | 12.00 | Bestellen | [Distrelec](https://www.distrelec.ch/de/search?q=BAT54S) (Suche) | AI1–AI8 |
| 44 | Strom | ATO-Sicherungshalter + 2 A | Inline | 1 | 6.00 | Bestellen | [Conrad](https://www.conrad.ch/de/search.html?search=Flachsicherungshalter%20inline) (Suche) | F2, COM K1–K4 |
| | | **Zwischensumme** | | | **873.50** | | | |
| | | Reserve 15 % | | | 131.03 | | | |
| | | **Planungsbudget** | | | **1004.53** | | | |

Davon jetzt bestellbar (Status Bestellen): CHF 469.60.

## Prüfstand (nicht im Fahrzeug)

| Teil | Zweck | CHF | Wo kaufen |
| --- | --- | ---: | --- |
| Labornetzteil 0–15 V / 5 A | Unit am Tisch, Unterspannung testen | vorhanden? | [Galaxus](https://www.galaxus.ch/de/search?q=Labornetzteil%2015V%205A) (Suche) |
| Prüfbrett: 8 Kippschalter (12 V), 4 Potis 0–2 kΩ, 4 LEDs | DI, Tankgeber/NTC und K1–K4 nachbilden | 25.00 | [Bastelgarage](https://www.bastelgarage.ch/) |
| USB-Logikanalysator 8 Kanäle (optional) | Pulse und Wahrheitstabelle mitschreiben | 20.00 | [Galaxus](https://www.galaxus.ch/de/search?q=Logic%20Analyzer%208%20Kanal) (Suche) |
| USB-Leistungsmesser oder Shunt-Messung 12 V | Ruhestrom messen (O19) | 25.00 | [Galaxus](https://www.galaxus.ch/de/search?q=USB%20Leistungsmesser) (Suche) |

## Hinweise aus der Recherche

- Pos. 1: 17.99 USD im Viewe-Shop, rund 33 USD inkl. Versand auf AliExpress (Stand März 2026, [CNX Software](https://www.cnx-software.com/2026/03/27/esp32-p4-pi-viewe-raspberry-pi-esp32-p4-esp32-c6-board/)).
- Pos. 5: Händler listen nur die HDMI-Version UEDX48128068L-HMD-H. Die MIPI-Version nur über Anfrage bei Viewe (O15).
- Pos. 19: Die verbreitete VW-Reparaturleitung 000979009E ist nicht sicher als MQS ausgewiesen. Teilenummer vor der Bestellung am Stecker prüfen.
- Pos. 29: Lieferzeit teils 12 Wochen. Die alte v1-Linkliste nannte SLD15U-017; gemäss HARDWARE.md gilt SLD15-018 (bidirektional).
- Noch nicht budgetiert (Menge nach Bauraumaufnahme): Pos. 31–35 und 37–40, Richtwert rund CHF 400, siehe HARDWARE.md.
