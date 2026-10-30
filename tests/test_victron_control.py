import pytest

from camperd.clock import FakeClock
from camperd.modules.energy import VictronStore
from camperd.modules.victron_control import VictronControl, VictronControlConfig


def make(enabled=True, lo=3.0, hi=16.0):
    clock = FakeClock()
    store = VictronStore(clock)
    store.connected, store.portal_id, store.vebus_instance = True, "p1", "276"
    store.ingest_vebus("Mode", 3)
    store.ingest_vebus("Ac/In/1/CurrentLimitIsAdjustable", 1)
    cfg = VictronControlConfig(enabled, (1, 3, 4), lo, hi, confirm_s=5, min_interval_s=2)
    return VictronControl(cfg, store, clock), store, clock


def test_disabled_by_default(profile):
    cfg = VictronControlConfig.from_profile(profile)
    assert not cfg.enabled and not cfg.limit_enabled  # real profile: control off, limits TODO_CHECK


def test_mode_write_and_confirm():
    ctl, store, clock = make()
    assert ctl.request({"mode": 1}) is None
    assert list(store.pending_writes) == [("Mode", 1)]
    store.ingest_vebus("Mode", 1)  # Cerbo reports the new mode
    clock.advance(1)
    ctl.tick()
    assert ctl.pending == {} and ctl.unconfirmed is None


def test_unconfirmed_mode_raises_alert():
    ctl, store, clock = make()
    ctl.request({"mode": 4})
    clock.advance(6)
    store.ingest_vebus("Mode", 3)  # still on: e.g. front switch not on "I"
    ctl.tick()
    assert ctl.unconfirmed == "mode"
    assert ctl.alerts() == [{"id": "victron_unconfirmed", "level": "warn"}]


@pytest.mark.parametrize("values,reason", [
    ({"mode": 2}, "bad_mode"),  # inverter only not in the allowed list
    ({"current_limit_a": 2.0}, "out_of_range"),
    ({"current_limit_a": 32}, "out_of_range"),
    ({}, "bad_message"),
])
def test_rejects(values, reason):
    ctl, _, _ = make()
    assert ctl.request(values) == reason


def test_rate_limit_and_offline():
    ctl, store, clock = make()
    assert ctl.request({"mode": 1}) is None
    assert ctl.request({"mode": 3}) == "too_fast"
    clock.advance(2.1)
    store.connected = False
    assert ctl.request({"mode": 3}) == "victron_offline"


def test_limit_needs_profile_values_and_adjustable_flag():
    ctl, _, _ = make(lo=None, hi=None)
    assert ctl.request({"current_limit_a": 10}) == "limit_disabled"
    ctl, store, _ = make()
    store.ingest_vebus("Ac/In/1/CurrentLimitIsAdjustable", 0)
    assert ctl.request({"current_limit_a": 10}) == "limit_not_adjustable"


def test_control_disabled():
    ctl, store, _ = make(enabled=False)
    assert ctl.request({"mode": 4}) == "control_disabled"
    assert not store.pending_writes


async def test_end_to_end_with_sim(rig):
    rig.cerbo.feed()
    assert rig.ctrl.command("victron", {"mode": 4}) is None
    rig.cerbo.feed()  # sim Cerbo forwards the write and reports back
    await rig.run_for(0.1)
    assert rig.victron.get("vebus.mode") == 4
    assert rig.victron.get("vebus.out_p") == 0
    assert rig.ctrl.victron_ctl.pending == {}
    rig.clock.advance(2.1)
    assert rig.ctrl.command("victron", {"current_limit_a": 6}) is None
    rig.cerbo.feed()
    assert rig.victron.get("vebus.current_limit") == 6.0
