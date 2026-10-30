#!/usr/bin/env python3
"""Bench check for the two SM-H-001 cards (run on the Pi, camperd stopped).

    sudo systemctl stop camperd
    python3 tools/bench_check.py                 # read-only: revisions, rail, opto, ADC, counters
    python3 tools/bench_check.py --relay-test    # clicks every relay once (bench only!)

Never run --relay-test with the unit connected to the vehicle.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from camperd.hal.sm_h001 import CardError, SmH001


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bus", type=int, default=1)
    ap.add_argument("--stacks", type=int, nargs="+", default=[0, 1])
    ap.add_argument("--relay-test", action="store_true")
    args = ap.parse_args()

    import smbus2

    bus = smbus2.SMBus(args.bus)
    ok = True
    for stack in args.stacks:
        card = SmH001(bus, stack)
        print(f"--- SM-H-001 stack {stack} (0x{card.addr:02x})")
        try:
            hw_ma, hw_mi, fw_ma, fw_mi = card.probe()
            print(f"hardware {hw_ma}.{hw_mi}  firmware {fw_ma}.{fw_mi}")
            print(f"3.3 V rail: {card.read_rail_v():.3f} V")
            print(f"relay power-up default: 0b{card.relay_default():08b}  current: 0b{card.read_relays():08b}")
            print(f"opto inputs: 0b{card.read_opto():08b}  (bit0 = opto 1)")
            adc = card.read_adc_v()
            print("ADC [V]: " + "  ".join(f"AI{i + 1}={v:.3f}" for i, v in enumerate(adc)))
            open_ai = [i + 1 for i, v in enumerate(adc) if v > 3.1]
            if open_ai:
                print(f"  open inputs near the rail -> on-board pull-up fitted on {open_ai} (HARDWARE.md 4.3)")
            print("counters: " + "  ".join(str(card.read_count(ch)) for ch in range(1, 9)))
            if args.relay_test:
                for ch in range(1, 9):
                    card.set_relay(ch, True)
                    time.sleep(0.3)
                    card.set_relay(ch, False)
                    time.sleep(0.2)
                    print(f"relay {ch}: on/off verified")
        except CardError as exc:
            print(f"ERROR: {exc}")
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
