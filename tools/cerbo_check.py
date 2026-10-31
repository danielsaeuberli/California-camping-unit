#!/usr/bin/env python3
"""Check MQTT to the Cerbo GX over its Wi-Fi access point (open item O19, blocking).

Run on the Pi with camperd stopped or running (it is only another MQTT client):

    python3 tools/cerbo_check.py                      # host from the profile (172.24.24.1)
    python3 tools/cerbo_check.py --host 172.24.24.1

Steps: connect -> read the portal id from N/<id>/system/0/Serial -> send an empty keepalive
to R/<id>/keepalive -> expect values (battery, VE.Bus). Exit code 0 = OK.
If MQTT does not answer over the AP: STOP and use the fallback (HARDWARE.md 5.3).
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

FALLBACK = (
    "STOP: MQTT does not answer over the Cerbo access point.\n"
    "Fallback (HARDWARE.md 5.3): small unmanaged switch, Cerbo + router + Pi on one subnet,\n"
    "static IPs router .1, Cerbo .10, Pi .20 (about 0.5-1.5 W extra). Report before changing anything."
)


async def check(host: str, port: int, portal: str | None, wait_s: float) -> int:
    import aiomqtt

    try:
        async with aiomqtt.Client(host, port, identifier="camper-check", timeout=5) as client:
            print(f"connected to {host}:{port}")
            await client.subscribe("N/+/system/0/Serial")
            if portal:
                await client.subscribe(f"N/{portal}/#")
                await client.publish(f"R/{portal}/keepalive", "")
            seen: dict[str, object] = {}
            deadline = time.monotonic() + wait_s
            async with asyncio.timeout(wait_s):
                async for msg in client.messages:
                    topic = str(msg.topic)
                    parts = topic.split("/")
                    if portal is None and topic.endswith("/system/0/Serial"):
                        portal = parts[1]
                        print(f"portal id: {portal}  -> sending keepalive")
                        await client.subscribe(f"N/{portal}/#")
                        await client.publish(f"R/{portal}/keepalive", "")
                        continue
                    if portal and topic.startswith(f"N/{portal}/"):
                        try:
                            seen[topic] = json.loads(msg.payload or b"{}").get("value")
                        except (ValueError, AttributeError):
                            seen[topic] = msg.payload
                    if time.monotonic() > deadline:  # collect everything: Orion XS / Ruuvi come late
                        break
    except TimeoutError:
        pass
    except aiomqtt.MqttError as exc:
        print(f"no MQTT connection to {host}:{port}: {exc}")
        print(FALLBACK)
        return 2

    if portal is None:
        print(f"no N/+/system/0/Serial within {wait_s:.0f} s (MQTT on LAN enabled on the Cerbo?)")
        print(FALLBACK)
        return 3
    if not seen:
        print(f"portal {portal} found, but no values after the keepalive")
        print(FALLBACK)
        return 4
    print(f"OK: {len(seen)} topics after the keepalive, e.g.:")
    for key in ("system/0/Dc/Battery/Soc", "system/0/Dc/Battery/Voltage", "system/0/Dc/Battery/TimeToGo",
                "system/0/Dc/Pv/Power", "system/0/Dc/Alternator/Power"):
        print(f"  {key} = {seen.get(f'N/{portal}/{key}', '-')}")
    vebus = sorted({t.split("/")[3] for t in seen if t.split("/")[2] == "vebus"})
    print(f"  VE.Bus instance(s): {', '.join(vebus) or '- (MultiPlus not on VE.Bus?)'}")
    services = sorted({"/".join(t.split("/")[2:4]) for t in seen})
    print(f"  services: {', '.join(services)}")  # O21: Orion XS as alternator/<n> or dcdc/<n>?
    for t in sorted(seen):  # O20: Ruuvi tags and their names
        if t.split("/")[2] == "temperature" and t.endswith(("/CustomName", "/ProductName", "/Temperature")):
            print(f"  {t.split('/', 2)[2]} = {seen[t]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=None, help="default: buses.cerbo.host from the profile")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--portal", default=None, help="portal id, if Serial discovery does not work")
    ap.add_argument("--wait", type=float, default=15.0)
    args = ap.parse_args()
    host = args.host
    if host is None:
        from camperd.profile import load_profile

        host = str(load_profile(ROOT / "profiles" / "t6-california-a8l-2018.yaml").require("buses.cerbo.host"))
    return asyncio.run(check(host, args.port, args.portal, args.wait))


if __name__ == "__main__":
    sys.exit(main())
