"""S2: K1 and K2 never together; reversal only after 300 ms with both off."""

from camperd.modules.roof import Roof, RoofConfig, RoofInputs

IDLE = RoofInputs(fb_a=False, fb_b=False, vehicle_moving=False, di_ok=True, ignition=True)


def make(sim_profile, clock):
    return Roof(RoofConfig.from_profile(sim_profile), clock)


def test_reversal_waits_for_gap(sim_profile, clock):
    roof = make(sim_profile, clock)
    roof.tick(IDLE)  # ignition on
    seq = 0
    for _ in range(5):
        seq += 1
        roof.hold("open", seq)
        roof.tick(IDLE)
        clock.advance(0.1)
    assert roof.k1 and not roof.k2
    # Switch direction immediately: both must be off for 300 ms first.
    elapsed = 0.0
    seen_both_off = False
    while elapsed < 0.6:
        seq += 1
        roof.hold("close", seq)
        k1, k2 = roof.tick(IDLE)
        assert not (k1 and k2)
        if elapsed < 0.29:
            assert not k1 and not k2
            seen_both_off = True
        clock.advance(0.05)
        elapsed += 0.05
    assert seen_both_off
    assert roof.k2 and not roof.k1


def test_release_then_other_direction_also_waits(sim_profile, clock):
    roof = make(sim_profile, clock)
    roof.tick(IDLE)  # ignition on
    roof.hold("open", 1)
    roof.tick(IDLE)
    assert roof.k1
    roof.release()
    roof.tick(IDLE)
    clock.advance(0.1)
    roof.hold("close", 2)
    assert roof.tick(IDLE) == (False, False)
    clock.advance(0.25)
    roof.hold("close", 3)
    assert roof.tick(IDLE) == (False, True)


async def test_controller_never_drives_both(rig):
    seq = 0
    for i in range(200):
        seq += 1
        rig.ctrl.roof_hold("open" if (i // 7) % 2 else "close", seq)
        await rig.run_for(0.02)
        assert not (rig.k[0] and rig.k[1])
