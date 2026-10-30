"""S9: losing the Cerbo changes nothing for roof, fridge and heater."""


async def test_cerbo_loss(rig):
    rig.ctrl.command("heater", {"on": True, "level": 5})
    rig.ctrl.command("fridge", {"on": True, "target_c": 3})
    await rig.run_for(1.0)
    before = (list(rig.k), list(rig.h), rig.ctrl.heater.phase, rig.ctrl.fridge.request)
    rig.hal.world.cerbo = False
    rig.cerbo.feed()
    rig.clock.advance(15)  # Victron values go stale
    await rig.run_for(0.2)
    rig.ctrl.roof_hold("open", 1)
    await rig.run_for(0.04)
    assert rig.victron.battery_v() is None
    assert rig.k[0], "roof still works"
    assert rig.k[3] == before[0][3]
    assert rig.ctrl.fridge.on and rig.ctrl.heater.on
