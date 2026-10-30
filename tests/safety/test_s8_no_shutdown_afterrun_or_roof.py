"""S8: no shutdown while the heater runs on or the roof moves."""

from camperd.power import PowerConfig, PowerInputs, PowerManager


def inputs(**kw):
    base = dict(ignition=False, fridge_active=False, heater_phase="off", roof_moving=False, battery_v=12.8)
    base.update(kw)
    return PowerInputs(**base)


def test_normal_path_reaches_shutdown(sim_profile, clock):
    pm = PowerManager(PowerConfig.from_profile(sim_profile), clock)
    pm.tick(inputs(ignition=True))
    pm.tick(inputs())
    assert pm.mode == "afterrun"
    clock.advance(61)
    assert pm.tick(inputs()) == "standby"
    clock.advance(1801)
    assert pm.tick(inputs()) == "shutdown"


def test_heater_afterrun_blocks_shutdown(sim_profile, clock):
    pm = PowerManager(PowerConfig.from_profile(sim_profile), clock)
    pm.tick(inputs())
    clock.advance(61)
    pm.tick(inputs(heater_phase="afterrun"))
    clock.advance(4000)
    assert pm.tick(inputs(heater_phase="afterrun")) == "standby"


def test_roof_moving_blocks_shutdown(sim_profile, clock):
    pm = PowerManager(PowerConfig.from_profile(sim_profile), clock)
    pm.tick(inputs())
    clock.advance(61)
    pm.tick(inputs(roof_moving=True))
    clock.advance(4000)
    assert pm.tick(inputs(roof_moving=True)) != "shutdown"


def test_undervoltage_forces_shutdown_except_afterrun(sim_profile, clock):
    pm = PowerManager(PowerConfig.from_profile(sim_profile), clock)
    pm.tick(inputs(battery_v=11.5, heater_phase="afterrun"))
    clock.advance(61)
    assert pm.tick(inputs(battery_v=11.5, heater_phase="afterrun")) != "shutdown"
    assert pm.undervoltage
    assert pm.tick(inputs(battery_v=11.5, fridge_active=True)) == "shutdown"


def test_no_cerbo_voltage_no_undervoltage_shutdown(sim_profile, clock):
    pm = PowerManager(PowerConfig.from_profile(sim_profile), clock)
    pm.user_input()
    for _ in range(10):
        clock.advance(10)
        pm.tick(inputs(battery_v=None, fridge_active=True))
    assert not pm.undervoltage
    assert pm.mode != "shutdown"
