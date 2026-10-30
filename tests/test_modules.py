from camperd.modules.heater import Heater, HeaterConfig


def test_heater_phases(sim_profile, clock):
    h = Heater(HeaterConfig.from_profile(sim_profile), clock)
    h.set(on=True)
    assert h.tick(False)[0] and h.phase == "start"
    h.tick(True)
    assert h.phase == "heating"
    h.set(on=False)
    assert h.phase == "afterrun"
    assert not h.tick(True)[0]
    clock.advance(100)
    assert h.afterrun_left_s() == 140
    h.tick(False)
    assert h.phase == "off"


def test_heater_start_timeout(sim_profile, clock):
    h = Heater(HeaterConfig.from_profile(sim_profile), clock)
    h.set(on=True)
    h.tick(False)
    clock.advance(121)
    k4, _ = h.tick(False)
    assert h.phase == "fault" and not k4


async def test_sim_tanks_and_temps(rig):
    await rig.run_for(12)
    st = rig.ctrl.state_data()
    assert st["tanks"]["fresh_step"] == 2
    assert st["tanks"]["waste_full"] is False
    assert abs(st["outside_c"] - 12.5) < 0.2
    assert st["roof"]["pos"] == "closed"
    rig.hal.world.waste_full = True
    await rig.run_for(17)
    assert rig.ctrl.state_data()["tanks"]["waste_full"] is True


async def test_uncalibrated_profile_shows_uncalibrated(profile, clock):
    from tests.conftest import Rig

    r = Rig(profile, clock)
    await r.ctrl.init_outputs()
    await r.run_for(1)
    st = r.ctrl.state_data()
    assert st["fridge"]["temp_q"] == "uncalibrated" and st["fridge"]["temp_c"] is None
    assert st["tanks"]["fresh_q"] == "uncalibrated" and st["tanks"]["fresh_step"] is None
    assert st["outside_c"] is None
    assert st["roof"]["pos"] == "unknown"


async def test_fridge_fault_code_end_to_end(rig):
    rig.hal.world.fridge_fault = 3
    await rig.run_for(13)
    assert rig.ctrl.fridge.fault == 3


async def test_roof_travel_in_sim(rig):
    seq = 0
    for _ in range(220):
        seq += 1
        rig.ctrl.roof_hold("open", seq)
        await rig.run_for(0.1)
    rig.ctrl.roof_release()
    await rig.run_for(0.2)
    assert rig.ctrl.roof.position() == "open"


async def test_speed_from_hardware_counter(rig):
    assert rig.ctrl.speed_counter
    rig.hal.world.moving = True
    await rig.run_for(1.2)
    assert rig.ctrl.vehicle.moving
    rig.hal.world.moving = False
    await rig.run_for(1.2)
    assert not rig.ctrl.vehicle.moving


async def test_open_sensor_and_external_voltage(rig):
    rig.hal.world.outside_c = -80  # NTC ~ open
    await rig.run_for(0.3)
    assert rig.ctrl.state_data()["outside_q"] in ("open", "implausible")


async def test_divider_overvoltage_alert(rig):
    orig = rig.hal.read_ai

    async def high():
        ai = await orig()
        ai[7] = 3.2  # AI8 battery divider above 3.0 V
        return ai

    rig.hal.read_ai = high
    await rig.run_for(0.2)
    assert any(a["id"] == "ai_overvoltage" for a in rig.ctrl.alerts())


async def test_58d_pwm_counts_as_on(rig):
    orig = rig.hal.read_di
    t = {"n": 0}

    async def pwm():
        di = await orig()
        t["n"] += 1
        di[6] = t["n"] % 10 == 0  # 58d high only every 10th sample (200 ms period, 10 % duty)
        return di

    rig.hal.read_di = pwm
    await rig.run_for(0.3)  # first high sample
    states = set()
    for _ in range(50):
        await rig.run_for(0.02)
        states.add(rig.ctrl.vehicle.dim58)
    assert states == {True}
