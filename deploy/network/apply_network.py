#!/usr/bin/env python3
"""Apply /data/network.json with nmcli (run as root by camper-network.service).

Runs at every boot (the NetworkManager files on the read-only root are lost on reboot)
and whenever camperd stores new settings. First setup without the touchscreen:

    sudo python3 deploy/network/apply_network.py --ssid "<venus-SSID>" --psk "<password>"
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from camperd.net import WIFI_CON, NetError, nm_commands, validate  # noqa: E402

CONFIG = Path("/data/network.json")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", type=Path, default=CONFIG)
    ap.add_argument("--ssid")
    ap.add_argument("--psk")
    ap.add_argument("--cerbo-host", default="172.24.24.1")
    args = ap.parse_args()

    if args.ssid:  # first setup from the command line
        cfg = {"eth": {"mode": "dhcp"}, "wifi": {"ssid": args.ssid, "psk": args.psk or ""},
               "cerbo": {"host": args.cerbo_host, "port": 1883}}
    elif args.config.exists():
        cfg = json.loads(args.config.read_text())
    else:
        print(f"{args.config} missing: run with --ssid/--psk once", file=sys.stderr)
        return 1
    try:
        cfg = validate(cfg)
    except NetError as exc:
        print(f"invalid network settings: {exc}", file=sys.stderr)
        return 2
    if args.ssid:
        args.config.parent.mkdir(parents=True, exist_ok=True)
        args.config.write_text(json.dumps(cfg, indent=2))
        args.config.chmod(0o600)
        subprocess.run(["chown", "camper:camper", str(args.config)], check=False)

    existing = WIFI_CON in subprocess.run(
        ["nmcli", "-t", "-f", "NAME", "con", "show"], capture_output=True, text=True, check=False
    ).stdout.split()
    failed = 0
    for cmd in nm_commands(cfg, existing_wifi=existing):
        res = subprocess.run(cmd, capture_output=True, text=True, check=False)
        shown = [("***" if i > 0 and cmd[i - 1] == "wifi-sec.psk" else a) for i, a in enumerate(cmd)]
        if res.returncode != 0:
            # "con up" may fail while a cable is unplugged or the Cerbo is off: not an error
            failed += "up" not in cmd
            print(f"failed: {' '.join(shown)}: {res.stderr.strip()}", file=sys.stderr)
        else:
            print(f"ok: {' '.join(shown)}")
    return 0 if failed == 0 else 3


if __name__ == "__main__":
    sys.exit(main())
