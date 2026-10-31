"""S2: K1 and K2 never together; reversal only after >= 300 ms with both off.

The pause is ``settle_s`` = max(reverse_gap 300 ms, pump run-on), so these tests use
the configured value (the sim profile has an invented 2 s run-on).
"""

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
    # Switch direction immediately: both must be off for settle_s (>= 300 ms) first.
    gap = roof.cfg.settle_s
    assert gap >= 0.3
    elapsed = 0.0
    seen_both_off = False
    while elapsed < gap + 0.3:
        seq += 1
        roof.hold("close", seq)
        k1, k2 = roof.tick(IDLE)
        assert not (k1 and k2)
        if elapsed < gap - 0.01:
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
    clock.advance(roof.cfg.settle_s - 0.05)
    roof.hold("close", 3)
    assert roof.tick(IDLE) == (False, True)


async def test_controller_never_drives_both(rig):
    seq = 0
    for i in range(200):
        seq += 1
        rig.ctrl.roof_hold("open" if (i // 7) % 2 else "close", seq)
        await rig.run_for(0.02)
        assert not (rig.k[0] and rig.k[1])
