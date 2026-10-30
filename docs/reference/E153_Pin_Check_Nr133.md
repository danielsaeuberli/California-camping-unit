# E153 pin check against VW wiring diagram Nr. 133

Version 0.1 | 27 October 2026 | Vehicle: VW T6 California 2.0 TDI 4Motion, 110 kW, VIN WV2ZZZ7HZJH197207 (MY 2018, 1st reg. 20.11.2018)

Source: *Stromlaufplan Grundausstattung ab Juni 2015* (K0058981600), **Nr. 133 "Campingausrüstung (A8L),(A8R) ab Juni 2017"**, sheets 133/2 to 133/11 (PDF pages 1037–1046, issue 12.2019). Compared with the signal registry in *Camping_Unit_Software_and_Frontend_Plan_v0_1.md* (section 3).

Wiring diagrams show connections, wire colours and cross-sections. They do not show signal polarity, active level, sensor curves or timing. "Confirmed" below means the connection matches; it says nothing about electrical behaviour.

Colour codes: br brown, sw black, rt red, ge yellow, gn green, bl blue, gr grey, vi/li violet, ws white, rs pink, or orange.

## T32b (32-pin, black, on E153)

| Pin | Plan says | Diagram shows (wire at E153 → route → destination) | Sheet | Result |
| --- | --- | --- | --- | --- |
| 01, 02, 04, 05, 06, 13, 23, 24 | Reserved | Not connected anywhere in Nr. 133 | – | Confirmed unused in this variant |
| 03 | Vehicle speed pulse | 0.5 ws/vi → T10u/5 → B464 "Verbindung (Geschwindigkeitssignal)" → J285 instrument cluster T32/9 | 133/10 | Confirmed (source J285) |
| 07 | G817 end A | 0.35 ws/br → T17b/11 → T6bj/6 → G817 pin 2 | 133/9 | Confirmed (lands on G817/2) |
| 08 | G817 end B | 0.35 gn/br → T17b/10 → T6bj/5 → G817 pin 1 | 133/9 | Confirmed (lands on G817/1) |
| 09 | Fridge diagnostic D | 0.5 ge/rt → T17b/8 → T6bj/4 → 1.0 ge → J698 fridge, terminal **D** | 133/9 | Confirmed |
| 10 | Fridge demand | 0.5 ge → T17b/7 → T6bj/3 → 1.5 rt → J1029 fridge control unit (7H7 068 455 E) **K4**; a resistor bridges this line to the J698 minus line | 133/9 | Confirmed destination; active level still open |
| 11 | Roof command A | 0.5 gn → T6bx/2 → 0.35 ws → **J768 roof hydraulics control unit** T32d/22 | 133/10 | Goes to J768, not to a roof motor |
| 12 | Roof command B | 0.5 ws → T6bx/1 → 0.35 br/bl → J768 T32d/23 | 133/10 | Goes to J768 |
| 14 | Roof feedback B | 0.5 ge/br → T6bx/6 → 0.35 or → J768 T32d/10 | 133/10 | Goes to J768 |
| 15 | Roof feedback A | 0.5 vi/br → T6bx/5 → 0.35 gr → J768 T32d/9 | 133/10 | Goes to J768 |
| 16 | G249 end A | 0.35 br → G249 pin 2 | 133/9 | Confirmed |
| 17 | G249 end B | 0.35 ge → G249 pin 1 | 133/9 | Confirmed |
| 18 | G126 waste end A | 0.35 br → T17b/16 → G126 pin 2 | 133/9 | Confirmed |
| 19 | G126 waste end B | 0.35 gn → T17b/13 → G126 pin 1 | 133/9 | Confirmed |
| 20 | G120 fresh end A | 0.35 br → T17b/15 → G120 pin 2 | 133/9 | Confirmed |
| 21 | G120 fresh end B | 0.35 bl → T17b/12 → G120 pin 1 | 133/9 | Confirmed |
| 22 | Charger A11 status | 0.5 vi → T17b/3 → T6bp/1 → 1.5 rt → A11 on-board charger | 133/3 | Confirmed destination; meaning still open |
| 25 | N162 sense C1 | 0.5 br → ground connection 290 → 1.5 br → N162 C1 (side joined to the camper-battery negatives at ground point 34) | 133/2 | Confirmed |
| 26 | N162 sense D1 | 0.5 sw → N162 D1 (body side, ground point 729 at rear left wheel arch) | 133/2 | Confirmed |
| 27 | Illumination 58d | 0.5 gr/bl → T10u/4 → B341 "Verbindung 2 (58d)" → E20 dimmer T6ac/5 (vehicles with dimmable instrument lighting) | 133/5 | Confirmed: 58d |
| 28 | Vehicle status B563 | 0.5 br/rt → B563 → T10u/3 → B518 **"Verbindung (86s)"** → J519 T73a/42; also fed via SC55 | 133/5 | Identified: terminal 86s |
| 29 | Vehicle status J519 | 0.5 sw → T10u/2 → B478 "Verbindung 14" → J519 T73a/3 | 133/5 | Destination confirmed; function still open (needs J519 pinout) |
| 30 | Ignition 15a | 0.5 sw/bl → T10u/1 → B284 "Plusverbindung 8 (15a)" → fuse SC34 | 133/5 | Confirmed: 15a via SC34 |
| 31 | Ground | 0.5 br → ground connection 449 (Leitungsstrang Camper) | 133/7 | Confirmed |
| 32 | Unit +12 V | 0.5 rt/ge → fuse **SH21 (5 A)** on holder H → B105 → B100 → **second battery A1 plus** | 133/3 | Confirmed: supply from the camper battery |

## T6ap (6-pin) and T2p (2-pin, on E153)

| Pin | Plan says | Diagram shows | Sheet | Result |
| --- | --- | --- | --- | --- |
| T6ap/1 | Supply | 0.5 rt → T10af/2 → B322 "Plusverbindung 8 (30a)" → fuse SC54 | 133/6 | Confirmed: 30a via SC54 |
| T6ap/2 | Heater operating indication | 0.35 ge/gr → T10af/8 → T10ba/2 → 0.5 rs → J604 | 133/6 | Goes to J604; function not shown |
| T6ap/3 | Heater S+ | 0.35 ws/ge → T10af/3 → T10ba/9 → 0.5 ge → J604 | 133/6 | Goes to J604; function not shown |
| T6ap/4 | Illumination variant | 0.35 gr/bl → T10af/6 → B340 "Verbindung 1 (58d)" → J519 T73b/4 (vehicles without dimmable instrument lighting) | 133/5 | Confirmed: 58d, alternative to pin 27 |
| T6ap/5 | Ground | 0.5 br → T10af/1 → ground 368 → 636 | 133/6 | Confirmed |
| T6ap/6 | Ignition variant | 0.35 sw → T10af/7 → B277 "Plusverbindung 1 (15a)" → fuse SC32 | 133/5 | Confirmed: 15a via SC32 |
| T2p/1 | Heater resistance | 0.35 rt/ws → T10af/9 → T10ba/7 → 0.35 gr/rt → J604 | 133/6 | Goes to J604 |
| T2p/2 | Heater resistance | 0.35 br/ws → T10af/10 → T10ba/10 → 0.35 br/sw → J604 | 133/6 | Goes to J604 |

## Findings that change the plan

1. **The roof has its own controller.** E153 pins 11, 12, 14 and 15 connect to **J768 Steuergerät für Dachhydraulik**. J768 drives the hydraulic pump V118 itself and reads four Hall sensors (G491–G494). So the E153 signals are requests to J768 and status signals back from it. They don't power the roof. That affects the relay design (signal level, not motor current) and the roof feedback model: J768 already knows the end positions, and pins 14/15 probably carry its status. Polarity and the truth table still need measuring.
2. **Battery voltage is available.** Pin 32 is fed from the second battery through SH21 (5 A). Measuring pin 32 against pin 31 gives camper-battery voltage, apart from a small drop across the fuse and wiring under load. The open question in plan section 1 ("confirm a battery-voltage measurement connection") can be closed for planning. Check the actual drop when installed.
3. **The shunt is on the low side.** N162 sits between the camper-battery negative (ground point 34) and body ground (729). Its common-mode voltage is close to 0 V, which suits the INA226. Sign convention still has to be measured.
4. **There are two ignition sources.** Pin 30 is 15a via SC34; T6ap/6 is 15a via SC32. Both are terminal 15a, so either works. Pick one in the hardware profile, as the plan already says.
5. **Which illumination input applies depends on equipment.** Pin 27 (via E20) is used with dimmable instrument lighting; T6ap/4 (via J519) without it. Check which wire is actually populated in your harness.
6. **Pin 28 is terminal 86s**, according to the connection name B518. Pin 29 goes to J519 T73a/3, but its function isn't stated in Nr. 133.
7. **The fridge is a compressor unit with its own controller.** Pin 10 goes to J1029 (the fridge control unit, part no. 7H7 068 455 E), not straight to the compressor. Pin 9 comes from terminal D of J698. Labelling the state "Cooling requested" rather than "Compressor running" stays right.
8. **The reserved pins match.** Contacts 1, 2, 4, 5, 6, 13, 23 and 24 are unused in this variant, exactly as the workbook says.

## Still open after the diagram check

* Active level and polarity of every output (10, 11, 12, T6ap/3) and input (9, 14, 15, 22, 28, 29, T6ap/2)
* Roof truth table for 14/15 and the J768 input logic for 11/12
* Function of pin 29 (J519 T73a/3)
* Which of T6ap/2 and T6ap/3 is S+ and which is the operating indication (the diagram shows both going to J604 but doesn't name the J604 pins)
* Calibration curves for G817, G249, G120, G126 and the N162 scale
* Whether your car has ambient lighting (9CE/9CI). If so, Nr. 145/146 may add connections.
* Confirming A8L or A8R on the car's PR-code label (vehicle data sticker / service book)
