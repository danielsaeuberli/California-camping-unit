"""Roof tap-to-run (one tap, STOP / any touch / end position) and pump run-on.

Tap-to-run still uses the 100 ms run messages, so every S3 guarantee holds: when the
messages stop (STOP, touch, frozen UI, socket loss) the relay drops within 300 ms.
"""

import itertools

import yaml

from camperd.modules.roof import Roof, RoofConfig, RoofInputs
from camperd.profile import Profile
from tests.conftest import PROFILE, SIM_OVERLAY


def inputs(a=False, b=False, moving=False, ignition=True):
    return RoofInputs(fb_a=a, fb_b=b, vehicle_moving=moving, di_ok=True, ignition=ignition)


CLOSED = inputs(a=False, b=True)   # sim truth table: "01" closed, "10" open, "00" between
BETWEEN = inputs()
OPEN = inputs(a=True, b=False)


SEQ = itertools.count(1)


def run(roof, clock, seconds, fb, direction="open", auto=True):
    out = None
    for _ in range(round(seconds / 0.1)):
        out = roof.hold(direction, next(SEQ), auto=auto)
        roof.tick(fb)
        clock.advance(0.1)
    return out


def test_tap_unavailable_without_measured_end_positions(profile, clock):
    roof = Roof(RoofConfig.from_profile(profile), clock)  # real profile: truth table TODO_MEASURE
    roof.tick(CLOSED)
    assert roof.hold("open", 1, auto=True) == "tap_unavailable"
    assert roof.tick(CLOSED) == (False, False)
    assert roof.state()["tap"] is False


def test_tap_runs_and_stops_at_end_position(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(CLOSED)
    assert roof.state()["tap"] is True
    run(roof, clock, 1.0, BETWEEN)
    assert roof.k1  # opening
    roof.hold("open", 10_000, auto=True)
    roof.tick(OPEN)  # J768 reports the open end position
    assert not roof.k1 and not roof.k2
    assert roof.hold("open", 10_001, auto=True) == "at_end"  # UI stops sending


def test_tap_at_end_does_nothing(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(OPEN)
    assert roof.hold("open", 1, auto=True) == "at_end"
    assert roof.tick(OPEN) == (False, False)


def test_tap_stops_when_run_messages_stop(sim_profile, clock):
    """STOP button, any touch, frozen UI or socket loss all end the run messages."""
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(CLOSED)
    run(roof, clock, 0.5, BETWEEN)
    assert roof.k1
    clock.advance(0.31)
    roof.tick(BETWEEN)
    assert not roof.k1


def test_tap_closing_stops_on_ignition_off_and_max_time(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.tick(OPEN)
    run(roof, clock, 0.5, BETWEEN, direction="close")
    assert roof.k2
    roof.hold("close", 20_000, auto=True)
    roof.tick(inputs(ignition=False))
    assert not roof.k2
    roof2 = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof2.tick(OPEN)
    run(roof2, clock, 60.2, BETWEEN, direction="close")
    assert not roof2.k2 and roof2.block == "timeout"


def make_with_afterrun(seconds):
    from camperd.profile.loader import _merge

    data = _merge(yaml.safe_load(PROFILE.read_text()), yaml.safe_load(SIM_OVERLAY.read_text()))
    data["roof"]["pump_afterrun_s"] = seconds
    return RoofConfig.from_profile(Profile(data))


def test_pump_afterrun_delays_reversal_and_counts_as_busy(clock):
    roof = Roof(make_with_afterrun(2.0), clock)
    roof.tick(CLOSED)
    roof.hold("open", 1)
    roof.tick(BETWEEN)
    assert roof.k1
    roof.release()
    roof.tick(BETWEEN)
    assert roof.afterrun and roof.busy and not roof.moving
    assert roof.state()["afterrun"] is True
    for i in range(15):  # 1.5 s: still in pump run-on, closing must wait
        clock.advance(0.1)
        roof.hold("close", 100 + i)
        assert roof.tick(BETWEEN) == (False, False)
    for i in range(6):
        clock.advance(0.1)
        roof.hold("close", 200 + i)
        roof.tick(BETWEEN)
    assert roof.k2 and not roof.afterrun


def test_unmeasured_afterrun_falls_back_to_reverse_gap(profile, clock):
    cfg = RoofConfig.from_profile(profile)
    assert cfg.pump_afterrun_s is None
    assert cfg.settle_s == cfg.reverse_gap_s


async def test_no_shutdown_during_pump_afterrun(rig):
    await rig.run_for(0.2)  # ignition debounced on
    assert rig.ctrl.roof_hold("open", 1) is None
    await rig.run_for(0.1)
    rig.ctrl.roof_release()
    await rig.run_for(0.1)
    assert rig.ctrl.roof.busy  # sim profile: 2 s run-on
    assert rig.ctrl.power.shutdown_blocked(
        __import__("camperd.power", fromlist=["PowerInputs"]).PowerInputs(
            ignition=False, fridge_active=False, heater_phase="off", roof_moving=rig.ctrl.roof.busy, battery_v=12.8
        )
    )
