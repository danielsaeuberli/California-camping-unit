"""S4: no opening while speed pulses are present; closing stays allowed; opening stops when driving starts."""

from camperd.modules.roof import Roof, RoofConfig, RoofInputs
from camperd.profile import load_profile
from tests.conftest import PROFILE

MOVING = RoofInputs(fb_a=False, fb_b=False, vehicle_moving=True, di_ok=True, ignition=True)
IDLE = RoofInputs(fb_a=False, fb_b=False, vehicle_moving=False, di_ok=True, ignition=True)


def test_open_rejected_close_allowed_when_moving(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(MOVING)
    assert roof.hold("open", 1) == "blocked_moving"
    assert roof.tick(MOVING) == (False, False)
    clock.advance(0.5)
    assert roof.hold("close", 2) is None
    k1, k2 = roof.tick(MOVING)
    assert k2 and not k1


def test_opening_stops_when_driving_starts(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(IDLE)  # ignition on
    roof.hold("open", 1)
    roof.tick(IDLE)
    assert roof.k1
    clock.advance(0.05)
    roof.hold("open", 2)
    roof.tick(MOVING)
    assert not roof.k1


def test_unverified_direction_blocks_both_when_moving(clock):
    """Until roof_a.meaning is measured, "close" might actually open: block both while moving."""
    roof = Roof(RoofConfig.from_profile(load_profile(PROFILE)), clock)
    roof.tick(MOVING)
    assert roof.hold("close", 1) == "blocked_moving"
    assert roof.tick(MOVING) == (False, False)


async def test_speed_signal_detected_by_controller(rig):
    rig.hal.world.moving = True
    await rig.run_for(1.2)
    assert rig.ctrl.vehicle.moving
    assert rig.ctrl.roof_hold("open", 1) == "blocked_moving"


NO_IGN = RoofInputs(fb_a=False, fb_b=False, vehicle_moving=False, di_ok=True, ignition=False)


def test_roof_requires_ignition(sim_profile, clock):
    """E153 releases the roof only with ignition on (research addendum 30.10.2026)."""
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(NO_IGN)
    assert roof.hold("open", 1) == "blocked_ignition"
    assert roof.hold("close", 2) == "blocked_ignition"
    assert roof.tick(NO_IGN) == (False, False)


def test_roof_stops_when_ignition_goes_off(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(IDLE)
    roof.hold("close", 1)
    assert roof.tick(IDLE) == (False, True)
    clock.advance(0.05)
    roof.hold("close", 2)
    assert roof.tick(NO_IGN) == (False, False)
    assert roof.block == "ignition"
