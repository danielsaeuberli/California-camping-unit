from __future__ import annotations

from camperd.clock import Clock
from camperd.hal.base import Hal, HalError
from camperd.profile import Profile


def create_hal(kind: str, profile: Profile, clock: Clock) -> Hal:
    if kind == "sim":
        from camperd.hal.sim import SimHal

        return SimHal(profile, clock)
    if kind == "hw":
        from camperd.hal.hw import HwHal

        return HwHal(profile)
    raise SystemExit(f"unknown HAL {kind!r}")


__all__ = ["Hal", "HalError", "create_hal"]
