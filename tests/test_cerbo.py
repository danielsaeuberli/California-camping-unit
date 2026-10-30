import json

from camperd.clock import FakeClock
from camperd.hal.cerbo_mqtt import KEEPALIVE_PAYLOAD, CerboClient, parse
from camperd.modules.energy import VictronStore


def msg(v):
    return json.dumps({"value": v}).encode()


def test_parse():
    assert parse("N/abc/system/0/Dc/Battery/Soc", msg(76)) == ("abc", "system", "0", "Dc/Battery/Soc", 76)
    assert parse("R/abc/keepalive", b"") is None
    assert parse("N/abc/system/0/Dc/Battery/Soc", b"garbage") is None


def test_discovery_and_ingest():
    store = VictronStore(FakeClock())
    c = CerboClient(store, "192.168.77.1")
    follow = c.handle("N/c0619ab1/system/0/Serial", msg("c0619ab1"))
    assert store.portal_id == "c0619ab1"
    assert "N/c0619ab1/system/0/Dc/#" in follow and "N/c0619ab1/vebus/+/#" in follow
    c.handle("N/c0619ab1/system/0/Dc/Battery/Soc", msg(76))
    c.handle("N/c0619ab1/vebus/276/ProductId", msg(9763))
    c.handle("N/c0619ab1/vebus/276/Ac/Out/L1/S", msg(800))
    c.handle("N/c0619ab1/vebus/999/Ac/Out/L1/S", msg(1))  # other instance ignored
    c.handle("N/other/system/0/Dc/Battery/Soc", msg(5))  # other portal ignored
    assert store.get("soc") == 76
    assert store.vebus_instance == "276"
    assert store.snapshot()["load_pct"] == 50


def test_keepalive_is_empty_and_fast_enough():
    import pytest

    from camperd.hal.cerbo_mqtt import keepalive_topic

    assert KEEPALIVE_PAYLOAD == ""  # empty -> Venus republishes every value
    assert keepalive_topic("c0619ab1") == "R/c0619ab1/keepalive"
    store = VictronStore(FakeClock(), stale_s=10)
    CerboClient(store, "172.24.24.1", keepalive_s=5)
    with pytest.raises(ValueError, match="stale_s"):
        CerboClient(store, "172.24.24.1", keepalive_s=30)  # steady values would go stale


def test_reconnect_after_cerbo_reboot_resubscribes_data():
    store = VictronStore(FakeClock())
    c = CerboClient(store, "172.24.24.1")
    assert c.on_connect() == ["N/+/system/0/Serial"]
    c.handle("N/c0619ab1/system/0/Serial", msg("c0619ab1"))
    c.handle("N/c0619ab1/vebus/276/ProductId", msg(9763))
    # Cerbo reboots: connection drops, client reconnects and must get the data topics again
    assert c.on_connect() == ["N/+/system/0/Serial"]
    assert store.portal_id is None and store.vebus_instance is None
    follow = c.handle("N/c0619ab1/system/0/Serial", msg("c0619ab1"))
    assert "N/c0619ab1/system/0/Dc/#" in follow and "N/c0619ab1/vebus/+/#" in follow
    c.handle("N/c0619ab1/vebus/276/ProductId", msg(9763))
    c.handle("N/c0619ab1/vebus/276/Mode", msg(3))
    assert store.get("vebus.mode") == 3


def test_fixed_portal_subscribes_immediately():
    store = VictronStore(FakeClock())
    c = CerboClient(store, "172.24.24.1", portal_id="c0619ab1")
    topics = c.on_connect()
    assert "N/c0619ab1/system/0/Dc/#" in topics and store.portal_id == "c0619ab1"


def test_values_go_stale_after_10_s_without_update():
    clock = FakeClock()
    store = VictronStore(clock, stale_s=10)
    c = CerboClient(store, "172.24.24.1", portal_id="p")
    c.handle("N/p/system/0/Dc/Battery/Soc", msg(76))
    clock.advance(9.9)
    assert store.get("soc") == 76
    clock.advance(0.2)
    assert store.get("soc") is None  # never show the last value
    c.handle("N/p/system/0/Dc/Battery/Soc", msg(76))  # republish after keepalive
    assert store.get("soc") == 76


def test_only_whitelisted_writes():
    import pytest

    from camperd.hal.cerbo_mqtt import write_topic

    assert write_topic("p1", "276", "Mode") == "W/p1/vebus/276/Mode"
    assert write_topic("p1", "276", "Ac/In/1/CurrentLimit") == "W/p1/vebus/276/Ac/In/1/CurrentLimit"
    for path in ("Settings/Something", "Hub4/L1/AcPowerSetpoint", "../system/0/Relay/0/State"):
        with pytest.raises(ValueError):
            write_topic("p1", "276", path)
