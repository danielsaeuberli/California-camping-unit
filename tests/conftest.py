from __future__ import annotations

from pathlib import Path

import pytest

from camperd.clock import FakeClock
from camperd.core import Controller
from camperd.hal.sim import SimCerbo, SimHal
from camperd.modules.energy import VictronStore
from camperd.profile import Profile, load_profile

ROOT = Path(__file__).resolve().parent.parent
PROFILE = ROOT / "profiles" / "t6-california-a8l-2018.yaml"
SIM_OVERLAY = ROOT / "profiles" / "sim" / "sim-calibration.yaml"


@pytest.fixture
def clock() -> FakeClock:
    return FakeClock()


@pytest.fixture
def profile() -> Profile:
    """The real vehicle profile (mostly uncalibrated)."""
    return load_profile(PROFILE)


@pytest.fixture
def sim_profile() -> Profile:
    return load_profile(PROFILE, overlay=SIM_OVERLAY)


class Rig:
    """Controller + simulator driven by a fake clock."""

    def __init__(self, profile: Profile, clock: FakeClock) -> None:
        self.clock = clock
        self.hal = SimHal(profile, clock)
        self.victron = VictronStore(clock)
        self.cerbo = SimCerbo(self.victron, self.hal.world)
        self.ctrl = Controller(profile, self.hal, clock, self.victron)

    async def run_for(self, seconds: float, step: float = 0.02) -> None:
        for _ in range(round(seconds / step)):
            self.clock.advance(step)
            await self.ctrl.cycle()

    @property
    def k(self) -> list[bool]:
        return self.hal.relays[:4]

    @property
    def h(self) -> list[bool]:
        return self.hal.gpio


@pytest.fixture
async def rig(sim_profile: Profile, clock: FakeClock) -> Rig:
    r = Rig(sim_profile, clock)
    await r.ctrl.init_outputs()
    r.cerbo.feed()
    return r
