#!/usr/bin/env python3
"""Bench check for SM-H-001 and SM-I-009 (run on the Pi, camperd stopped).

    sudo systemctl stop camperd
    python3 tools/bench_check.py                 # read-only: revisions, rail, relays, ADC, inputs
    python3 tools/bench_check.py --relay-test    # clicks every SM-H-001 relay once (bench only!)

Never run --relay-test with the unit connected to the vehicle.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from camperd.hal.sm_h001 import CardError, SmH001
from camperd.hal.sm_i009 import SmI009


def check_h001(card: SmH001, relay_test: bool) -> None:
    print(f"--- SM-H-001 stack {card.stack} (0x{card.addr:02x})")
    hw_ma, hw_mi, fw_ma, fw_mi = card.probe()
    print(f"hardware {hw_ma}.{hw_mi}  firmware {fw_ma}.{fw_mi}")
    print(f"3.3 V rail: {card.read_rail_v():.3f} V")
    print(f"relay power-up default: 0b{card.relay_default():08b}  current: 0b{card.read_relays():08b}")
    adc = card.read_adc_v()
    print("ADC [V]: " + "  ".join(f"AI{i + 1}={v:.3f}" for i, v in enumerate(adc)))
    open_ai = [i + 1 for i, v in enumerate(adc) if v > 3.1]
    if open_ai:
        print(f"  open inputs near the rail -> on-board pull-up fitted on AI{open_ai} (HARDWARE.md 4.3)")
    if relay_test:
        for ch in range(1, 9):
            card.set_relay(ch, True)
            time.sleep(0.3)
            card.set_relay(ch, False)
            time.sleep(0.2)
            print(f"relay K{ch}: on/off verified")


def check_i009(card: SmI009) -> None:
    print(f"--- SM-I-009 stack {card.stack} (0x{card.addr:02x})")
    rev = card.probe()
    print(f"hardware {rev[0]}.{rev[1]}  firmware {rev[2]}.{rev[3]}" if rev else "V2 (I/O expander, no counters)")
    di = card.read_inputs()
    print("inputs: " + " ".join(f"DI{i + 1}={int(v)}" for i, v in enumerate(di)))
    if card.has_counters:
        print("counters: " + "  ".join(str(card.read_count(ch)) for ch in range(1, 17)))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bus", type=int, default=1)
    ap.add_argument("--h001-stack", type=int, default=0)
    ap.add_argument("--i009-stack", type=int, default=0)
    ap.add_argument("--relay-test", action="store_true")
    args = ap.parse_args()

    import smbus2

    bus = smbus2.SMBus(args.bus)
    ok = True
    for check in (
        lambda: check_h001(SmH001(bus, args.h001_stack), args.relay_test),
        lambda: check_i009(SmI009(bus, args.i009_stack)),
    ):
        try:
            check()
        except CardError as exc:
            print(f"ERROR: {exc}")
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
