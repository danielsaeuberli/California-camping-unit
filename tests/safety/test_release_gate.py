"""Functions whose measurements are pending (profile: release.*: false) never switch."""

from tests.conftest import Rig


async def test_real_profile_releases_nothing(profile, clock):
    r = Rig(profile, clock)
    r.hal.world.ignition = True
    await r.ctrl.init_outputs()
    await r.run_for(0.2)
    assert r.ctrl.released == {"roof": False, "fridge": False, "heater": False}
    assert r.ctrl.roof_hold("open", 1) == "not_released"
    assert r.ctrl.command("heater", {"on": True}) == "not_released"
    assert r.ctrl.command("fridge", {"on": True}) == "not_released"
    # even if a module asked for it, the output stays off
    r.ctrl.heater.set(on=True)
    r.ctrl.fridge.set(on=True)
    await r.run_for(1.0)
    assert r.hal.relays == [False] * 8 and r.hal.gpio == [False] * 4


async def test_sim_profile_releases_all(rig):
    assert all(rig.ctrl.released.values())
    assert rig.ctrl.command("heater", {"on": True}) is None
