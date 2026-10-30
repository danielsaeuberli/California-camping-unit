"""S10: watchdog switches all outputs off and terminates when the cycle stalls > 2 s."""

from camperd.clock import FakeClock
from camperd.hal.sim import SimHal
from camperd.power import Watchdog


def test_trips_after_two_seconds(sim_profile):
    clock = FakeClock()
    hal = SimHal(sim_profile, clock)
    hal.relays = [True] * 8
    hal.gpio = [True] * 4
    exited = []
    wd = Watchdog(hal.all_off_sync, timeout_s=2.0, on_trip=lambda: exited.append(True), now=clock.now)
    clock.advance(1.9)
    assert not wd.check()
    wd.kick()
    clock.advance(1.9)
    assert not wd.check()
    clock.advance(0.2)
    assert wd.check()
    assert exited == [True]
    assert hal.relays == [False] * 8 and hal.gpio == [False] * 4


def test_runs_in_thread_and_survives_blocked_loop(sim_profile):
    import time

    clock = FakeClock()
    hal = SimHal(sim_profile, clock)
    hal.relays = [True] * 8
    tripped = []
    wd = Watchdog(hal.all_off_sync, timeout_s=0.3, on_trip=lambda: tripped.append(True))
    wd.start()
    time.sleep(0.8)  # the "event loop" blocks here; the watchdog thread still runs
    wd.stop()
    assert tripped and hal.relays == [False] * 8
