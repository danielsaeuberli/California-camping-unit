"""Entry point: ``python -m camperd [--hal sim|hw]``."""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import logging
import os
import signal
from pathlib import Path

from aiohttp import web

from camperd.api.server import Api
from camperd.api.store import EventLog, Settings
from camperd.clock import SystemClock
from camperd.core import Controller
from camperd.hal import create_hal
from camperd.hal.cerbo_mqtt import CerboClient
from camperd.hal.sim import SimCerbo, SimHal
from camperd.modules.energy import VictronStore
from camperd.power import Watchdog
from camperd.profile import load_profile

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE = ROOT / "profiles" / "t6-california-a8l-2018.yaml"
SIM_OVERLAY = ROOT / "profiles" / "sim" / "sim-calibration.yaml"

log = logging.getLogger("camperd")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    ap = argparse.ArgumentParser(prog="camperd")
    ap.add_argument("--hal", default=os.environ.get("CAMPERD_HAL", "sim"), choices=("sim", "hw"))
    ap.add_argument("--profile", type=Path, default=None, help="default: /data/profile.yaml if present, else template")
    ap.add_argument("--data-dir", type=Path, default=Path(os.environ.get("CAMPERD_DATA", ROOT / "var")))
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--ui-dir", type=Path, default=ROOT / "ui")
    ap.add_argument(
        "--sim-uncalibrated",
        action="store_true",
        help="simulator without the invented sim calibration (shows 'nicht kalibriert')",
    )
    ap.add_argument("--no-watchdog", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    return ap.parse_args(argv)


def resolve_profile(args: argparse.Namespace) -> Path:
    if args.profile:
        return Path(args.profile)
    calibrated = args.data_dir / "profile.yaml"
    return calibrated if calibrated.exists() else DEFAULT_PROFILE


async def run(args: argparse.Namespace) -> None:
    overlay = SIM_OVERLAY if args.hal == "sim" and not args.sim_uncalibrated else None
    profile = load_profile(resolve_profile(args), overlay=overlay)
    log.info("profile %s, hal %s", profile.source, args.hal)

    clock = SystemClock()
    hal = create_hal(args.hal, profile, clock)
    victron = VictronStore(clock, stale_s=float(profile.get("buses.cerbo.stale_s", 10)))
    ctrl = Controller(profile, hal, clock, victron)

    await hal.start()
    await ctrl.init_outputs()  # S1 before the API accepts anything

    cerbo: SimCerbo | None = None
    if isinstance(hal, SimHal):
        cerbo = SimCerbo(victron, hal.world)
        await cerbo.start()
    cerbo_hw: CerboClient | None = None
    if args.hal == "hw":
        cerbo_hw = CerboClient(
            victron,
            str(profile.require("buses.cerbo.host")),
            int(profile.get("buses.cerbo.port", 1883)),
            float(profile.get("buses.cerbo.keepalive_s", 30)),
        )
        await cerbo_hw.start()

    settings = Settings(args.data_dir / "settings.json", profile)
    events = EventLog(args.data_dir / "log" / "events.jsonl")
    api = Api(ctrl, clock, settings, events, args.ui_dir)
    runner = web.AppRunner(api.app())
    await runner.setup()
    await web.TCPSite(runner, args.host, args.port).start()
    log.info("UI on http://%s:%d/", args.host, args.port)

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        with contextlib.suppress(NotImplementedError):
            loop.add_signal_handler(sig, stop.set)

    watchdog = Watchdog(hal.all_off_sync)
    if not args.no_watchdog:
        watchdog.start()
    try:
        await ctrl.run(stop, kick=watchdog.kick)
    finally:
        watchdog.stop()
        if cerbo:
            await cerbo.stop()
        if cerbo_hw:
            await cerbo_hw.stop()
        await runner.cleanup()
        hal.all_off_sync()
        await hal.stop()


def main(argv: list[str] | None = None) -> None:
    args = parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    asyncio.run(run(args))
