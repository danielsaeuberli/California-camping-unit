"""S1: K1-K4 off and H1-H4 unpowered before camperd accepts any message."""

from camperd.clock import FakeClock
from camperd.core import Controller
from camperd.hal.sim import SimHal
from camperd.modules.energy import VictronStore


async def test_init_switches_everything_off(sim_profile):
    clock = FakeClock()
    hal = SimHal(sim_profile, clock)
    hal.relays = [True] * 8  # e.g. left on by a crashed previous run
    hal.gpio = [True] * 4
    ctrl = Controller(sim_profile, hal, clock, VictronStore(clock))
    assert not ctrl.ready
    await ctrl.init_outputs()
    assert ctrl.ready
    assert hal.relays == [False] * 8
    assert hal.gpio == [False] * 4


async def test_ladder_unpowered_means_base_resistance(rig):
    await rig.run_for(0.5)
    assert rig.h == [False] * 4
    assert rig.ctrl.heater.ladder_ohm() == rig.ctrl.heater.cfg.base_ohm


def test_uncalibrated_ladder_rejects_level(profile, clock):
    from camperd.modules.heater import Heater, HeaterConfig

    h = Heater(HeaterConfig.from_profile(profile), clock)
    assert h.set(level=5) == "ladder_uncalibrated"
    h.set(on=True)
    for _ in range(10):
        _, bits = h.tick(False)
        clock.advance(0.06)
    assert bits == [False] * 4 and h.ladder_ohm() is None
