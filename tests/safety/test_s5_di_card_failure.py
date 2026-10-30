"""S5: three DI read failures in a row -> DI stale, roof blocked, alert."""


async def test_di_failure_blocks_roof(rig):
    await rig.run_for(0.1)
    rig.ctrl.roof_hold("open", 1)
    await rig.run_for(0.04)
    assert rig.k[0]
    rig.hal.world.di_card_fail = True
    await rig.run_for(0.02 * 2)
    assert rig.ctrl.di_ok  # two failures are tolerated
    rig.ctrl.roof_hold("open", 2)
    await rig.run_for(0.02)
    assert not rig.ctrl.di_ok
    assert not rig.k[0]
    assert all(v is None for v in rig.ctrl.di)
    assert rig.ctrl.roof_hold("open", 3) == "blocked_io"
    assert any(a["id"] == "di_card" for a in rig.ctrl.alerts())


async def test_recovers_after_card_returns(rig):
    rig.hal.world.di_card_fail = True
    await rig.run_for(0.1)
    rig.hal.world.di_card_fail = False
    await rig.run_for(0.1)
    assert rig.ctrl.di_ok
    assert rig.ctrl.roof_hold("open", 1) is None
