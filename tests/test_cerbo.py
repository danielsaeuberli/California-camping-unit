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


def _ruuvi(c, inst, name, temp, product="RuuviTag"):
    c.handle(f"N/p1/temperature/{inst}/ProductName", msg(product))
    c.handle(f"N/p1/temperature/{inst}/CustomName", msg(name))
    c.handle(f"N/p1/temperature/{inst}/Temperature", msg(temp))
    c.handle(f"N/p1/temperature/{inst}/Humidity", msg(48.4))


def _client():
    clock = FakeClock()
    store = VictronStore(clock)
    c = CerboClient(store, "172.24.24.1", portal_id="p1")
    store.connected = True
    return clock, store, c


def test_temperature_topics_subscribed():
    _, _store, c = _client()
    assert "N/p1/temperature/+/#" in c.on_connect()


def test_interior_single_ruuvi_without_name():
    _, store, c = _client()
    _ruuvi(c, "20", "Ruuvi 1A2B", 21.37)
    c.handle("N/p1/temperature/21/ProductName", msg("Temperature sensor"))  # wired sensor, not a Ruuvi
    c.handle("N/p1/temperature/21/Temperature", msg(5.0))
    assert store.interior(None) == {"c": 21.4, "rh": 48, "name": "Ruuvi 1A2B", "q": "ok"}


def test_interior_by_custom_name():
    _, store, c = _client()
    _ruuvi(c, "20", "Kuehlbox", 4.0)
    _ruuvi(c, "21", "Innen", 19.0)
    assert store.interior(None)["q"] == "ambiguous"  # never guess between two tags
    assert store.interior("innen")["c"] == 19.0
    assert store.interior("Bad")["q"] == "missing"


def test_interior_stale_and_offline():
    clock, store, c = _client()
    _ruuvi(c, "20", "Innen", 19.0)
    clock.advance(store.stale_s + 1)
    assert store.interior("Innen") == {"c": None, "rh": None, "name": None, "q": "missing"}
    c.handle("N/p1/temperature/20/CustomName", msg("Innen"))  # service still there, no reading
    assert store.interior("Innen")["q"] == "stale"
    store.connected = False
    assert store.interior("Innen")["q"] == "offline"


def test_interior_rejects_non_numbers():
    _, store, c = _client()
    _ruuvi(c, "20", "Innen", None)
    assert store.interior("Innen")["c"] is None
    c.handle("N/p1/temperature/20/Temperature", msg(True))
    assert store.interior("Innen")["c"] is None


def test_alternator_power_from_system():
    _, store, c = _client()
    c.handle("N/p1/system/0/Dc/Alternator/Power", msg(452))
    assert store.get("alternator_w") == 452
    assert store.snapshot()["alternator_w"]["val"] == 452


def test_starter_battery_from_orion_input():
    clock, store, c = _client()
    assert "N/p1/alternator/+/Dc/In/V" in c.data_topics()
    assert store.snapshot()["starter_v"] is None
    c.handle("N/p1/alternator/277/Dc/In/V", msg(12.452))
    assert store.snapshot()["starter_v"]["val"] == 12.45
    clock.advance(store.stale_s + 1)
    assert store.starter_v() is None  # stale -> "–"


def test_starter_battery_ambiguous_with_two_dcdc():
    _, store, c = _client()
    c.handle("N/p1/alternator/277/Dc/In/V", msg(12.4))
    c.handle("N/p1/dcdc/280/Dc/In/V", msg(13.1))
    assert store.starter_v() is None
