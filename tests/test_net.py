import json
import stat

import pytest

from camperd.net import NetError, NetworkSettings, nm_commands, validate

GOOD = {
    "eth": {"mode": "dhcp"},
    "wifi": {"ssid": "venus-HQ2207ABCDE-3a4", "psk": "abcdefgh12"},
    "cerbo": {"host": "172.24.24.1", "port": 1883},
}


def cfg(**over):
    c = json.loads(json.dumps(GOOD))
    for section, vals in over.items():
        c[section].update(vals)
    return c


def test_valid_dhcp_and_static():
    assert validate(GOOD)["eth"] == {"mode": "dhcp", "address": "", "gateway": "", "dns": []}
    st = validate(cfg(eth={"mode": "static", "address": "192.168.1.20/24", "gateway": "192.168.1.1", "dns": ["1.1.1.1"]}))
    assert st["eth"]["address"] == "192.168.1.20/24" and st["eth"]["dns"] == ["1.1.1.1"]


@pytest.mark.parametrize("over,reason", [
    ({"eth": {"mode": "static", "address": "192.168.1.20"}}, "net_eth_address"),  # no prefix -> /32
    ({"eth": {"mode": "static", "address": "192.168.1.0/24"}}, "net_eth_address"),  # network address
    ({"eth": {"mode": "static", "address": "192.168.1.20/24", "gateway": "10.0.0.1"}}, "net_eth_gateway"),
    ({"eth": {"mode": "static", "address": "192.168.1.20/24", "dns": ["nope"]}}, "net_eth_dns"),
    ({"eth": {"mode": "bridge"}}, "net_eth_mode"),
    ({"wifi": {"ssid": ""}}, "net_wifi_ssid"),
    ({"wifi": {"psk": "short"}}, "net_wifi_psk"),
    ({"cerbo": {"host": "cerbo.local"}}, "net_cerbo_host"),
    ({"cerbo": {"port": 0}}, "net_cerbo_port"),
    ({"eth": {"mode": "static", "address": "172.24.24.20/24"}}, "net_cerbo_on_eth"),
])
def test_rejects(over, reason):
    with pytest.raises(NetError, match=reason):
        validate(cfg(**over))


def test_nm_commands_no_shell_and_wifi_never_default():
    cmds = nm_commands(validate(cfg(eth={"mode": "static", "address": "192.168.1.20/24", "gateway": "192.168.1.1"})))
    assert all(isinstance(c, list) and c[0] == "nmcli" for c in cmds)
    wifi = cmds[0]
    assert wifi[wifi.index("ipv4.never-default") + 1] == "yes"
    assert wifi[wifi.index("wifi-sec.psk") + 1] == "abcdefgh12"  # an argument, never shell-interpolated
    eth = cmds[1]
    assert eth[eth.index("ipv4.method") + 1] == "manual" and eth[eth.index("ipv4.route-metric") + 1] == "100"


async def test_update_stores_hides_password_and_keeps_it(tmp_path, profile):
    applied = []

    async def apply():
        applied.append(True)

    net = NetworkSettings(tmp_path / "network.json", profile, apply)
    await net.update(GOOD)
    assert applied == [True]
    path = tmp_path / "network.json"
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    pub = net.public()
    assert "psk" not in pub["wifi"] and pub["wifi"]["psk_set"] is True
    await net.update({"eth": {"mode": "static", "address": "10.0.0.5/24"}})  # no password sent
    assert json.loads(path.read_text())["wifi"]["psk"] == "abcdefgh12"
    with pytest.raises(NetError):
        await net.update({"eth": {"mode": "static", "address": "bad"}})
    assert json.loads(path.read_text())["eth"]["address"] == "10.0.0.5/24"  # unchanged on error
    reloaded = NetworkSettings(path, profile)
    assert reloaded.cfg["eth"]["address"] == "10.0.0.5/24"


async def test_cerbo_change_notifies(tmp_path, profile):
    seen = []
    net = NetworkSettings(tmp_path / "n.json", profile)
    net.on_change.append(lambda c: seen.append(c["cerbo"]["host"]))
    await net.update(GOOD)
    await net.update({"cerbo": {"host": "172.24.24.2"}})
    assert seen == ["172.24.24.2"]


async def test_api_net_messages(rig, tmp_path):
    from aiohttp.test_utils import TestClient, TestServer

    from camperd.api.server import Api
    from camperd.api.store import EventLog, Settings
    from tests.conftest import ROOT
    from tests.test_api import recv_type

    net = NetworkSettings(tmp_path / "network.json", rig.ctrl.p)
    api = Api(rig.ctrl, rig.clock, Settings(tmp_path / "s.json", rig.ctrl.p), EventLog(tmp_path / "e.jsonl"),
              ROOT / "ui", net=net)
    async with TestClient(TestServer(api.app())) as client:
        ws = await client.ws_connect("/ws")
        await ws.send_str(json.dumps({"v": 1, "type": "net_get"}))
        msg = await recv_type(ws, "net")
        assert msg["data"]["config"]["cerbo"]["host"] == "172.24.24.1"
        await ws.send_str(json.dumps({"v": 1, "type": "net_set", "id": "n1", "set": GOOD}))
        assert (await recv_type(ws, "ack"))["ok"]
        msg = await recv_type(ws, "net")
        assert msg["data"]["config"]["wifi"] == {"ssid": GOOD["wifi"]["ssid"], "psk_set": True}
        await ws.send_str(json.dumps({"v": 1, "type": "net_set", "id": "n2", "set": {"wifi": {"psk": "x"}}}))
        ack = await recv_type(ws, "ack")
        assert not ack["ok"] and ack["reason"] == "net_wifi_psk"
    assert "abcdefgh12" not in (tmp_path / "e.jsonl").read_text()  # password never logged
