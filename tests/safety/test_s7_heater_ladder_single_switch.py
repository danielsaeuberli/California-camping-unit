"""S7: heater ladder relays switch one at a time, 50 ms apart."""

import itertools

from camperd.modules.heater import Heater, HeaterConfig


def test_one_relay_per_50ms(sim_profile, clock):
    h = Heater(HeaterConfig.from_profile(sim_profile), clock)
    h.set(on=True, level=15)
    prev = [False] * 4
    changes = []
    for _ in range(40):
        _, bits = h.tick(False)
        diff = sum(a != b for a, b in zip(prev, bits, strict=True))
        assert diff <= 1
        if diff:
            changes.append(clock.now())
        prev = bits
        clock.advance(0.01)
    assert prev == [True] * 4
    gaps = [b - a for a, b in itertools.pairwise(changes)]
    assert all(g >= 0.05 - 1e-9 for g in gaps)


def test_level_to_bits_and_resistance(sim_profile, clock):
    cfg = HeaterConfig.from_profile(sim_profile)
    h = Heater(cfg, clock)
    h.set(on=True, level=0b1010)
    for _ in range(20):
        h.tick(False)
        clock.advance(0.06)
    assert h.ladder.bits == [False, True, False, True]
    g = 1 / cfg.base_ohm + 1 / cfg.branch_ohm[1] + 1 / cfg.branch_ohm[3]
    assert abs(h.ladder_ohm() - 1 / g) < 1e-6


async def test_controller_ladder_relays_single_switch(rig):
    rig.ctrl.command("heater", {"on": True, "level": 15})
    prev = list(rig.h)
    for _ in range(30):
        await rig.run_for(0.02)
        assert sum(a != b for a, b in zip(prev, rig.h, strict=True)) <= 1
        prev = list(rig.h)
