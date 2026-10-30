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


def test_keepalive_is_a_read_request():
    assert "suppress-republish" in KEEPALIVE_PAYLOAD
    from tests.conftest import ROOT

    src = (ROOT / "camperd" / "hal" / "cerbo_mqtt.py").read_text()
    assert '"W/' not in src and "'W/" not in src and 'f"W/' not in src
