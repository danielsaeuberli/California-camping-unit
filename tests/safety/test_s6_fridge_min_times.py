"""S6: fridge minimum run time 2 min, minimum pause 3 min."""

from camperd.modules.fridge import Fridge, FridgeConfig
from camperd.signals import Quality, Value


def v(t, clock):
    return Value(t, clock.now(), Quality.OK)


def test_min_run_and_pause(sim_profile, clock):
    f = Fridge(FridgeConfig.from_profile(sim_profile), clock)
    f.set(on=True, target_c=5)
    assert f.tick(v(7.0, clock), None)  # 7 >= 5 + 1.5
    clock.advance(30)
    assert f.tick(v(3.0, clock), None), "must keep running for 120 s"
    clock.advance(89)
    assert f.tick(v(3.0, clock), None)
    clock.advance(2)
    assert not f.tick(v(3.0, clock), None)
    clock.advance(60)
    assert not f.tick(v(8.0, clock), None), "must pause for 180 s"
    clock.advance(121)
    assert f.tick(v(8.0, clock), None)


def test_switching_off_respects_min_run(sim_profile, clock):
    f = Fridge(FridgeConfig.from_profile(sim_profile), clock)
    f.set(on=True)
    f.tick(v(8.0, clock), None)
    clock.advance(10)
    f.set(on=False)
    assert f.tick(v(8.0, clock), None)
    clock.advance(111)
    assert not f.tick(v(8.0, clock), None)


def test_hysteresis(sim_profile, clock):
    f = Fridge(FridgeConfig.from_profile(sim_profile), clock)
    f.set(on=True, target_c=5)
    assert not f.tick(v(6.4, clock), None)
    clock.advance(200)
    assert f.tick(v(6.5, clock), None)
    clock.advance(200)
    assert f.tick(v(4.1, clock), None)
    assert not f.tick(v(4.0, clock), None)


def test_sensor_fault_stops_request(sim_profile, clock):
    f = Fridge(FridgeConfig.from_profile(sim_profile), clock)
    f.set(on=True)
    f.tick(v(9.0, clock), None)
    clock.advance(130)
    assert not f.tick(Value(None, clock.now(), Quality.OPEN), None)
    assert any(a["id"] == "fridge_sensor" for a in f.alerts())


def test_uncalibrated_never_requests(profile, clock):
    f = Fridge(FridgeConfig.from_profile(profile), clock)
    f.set(on=True)
    assert not f.tick(Value(None, clock.now(), Quality.UNCALIBRATED), None)
