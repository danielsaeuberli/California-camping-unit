import asyncio
import json

from aiohttp.test_utils import TestClient, TestServer

from camperd.api.server import Api
from camperd.api.store import EventLog, Settings
from tests.conftest import ROOT


async def client_for(rig, tmp_path):
    api = Api(rig.ctrl, rig.clock, Settings(tmp_path / "s.json", rig.ctrl.p), EventLog(tmp_path / "e.jsonl"), ROOT / "ui")
    return api, TestClient(TestServer(api.app()))


async def recv_type(ws, kind, timeout=2.0):
    async def loop():
        while True:
            m = json.loads((await ws.receive()).data)
            if m["type"] == kind:
                return m
    return await asyncio.wait_for(loop(), timeout)


async def test_hello_state_and_cmd_ack(rig, tmp_path):
    _, client = await client_for(rig, tmp_path)
    async with client:
        ws = await client.ws_connect("/ws")
        assert (await recv_type(ws, "hello"))["data"]["hal"] == "sim"
        st = await recv_type(ws, "state")
        assert st["v"] == 1 and "roof" in st["data"]
        await ws.send_str(json.dumps({"v": 1, "type": "cmd", "id": "c-1", "target": "fridge", "set": {"target_c": 42}}))
        ack = await recv_type(ws, "ack")
        assert ack == {"v": 1, "type": "ack", "id": "c-1", "ok": False, "reason": "out_of_range"}
        await ws.send_str(json.dumps({"v": 1, "type": "cmd", "id": "c-2", "target": "heater", "set": {"on": True, "level": 8}}))
        ack = await recv_type(ws, "ack")
        assert ack["ok"] is True
        assert rig.ctrl.heater.on


async def test_hold_rejected_while_moving(rig, tmp_path):
    rig.hal.world.moving = True
    await rig.run_for(1.2)
    _, client = await client_for(rig, tmp_path)
    async with client:
        ws = await client.ws_connect("/ws")
        await ws.send_str(json.dumps({"v": 1, "type": "hold", "target": "roof", "dir": "open", "seq": 1}))
        ack = await recv_type(ws, "ack")
        assert ack["reason"] == "blocked_moving"


async def test_io_only_after_subscribe(rig, tmp_path):
    _, client = await client_for(rig, tmp_path)
    async with client:
        ws = await client.ws_connect("/ws")
        await ws.send_str(json.dumps({"v": 1, "type": "subscribe", "topics": ["io"]}))
        io = await recv_type(ws, "io")
        assert len(io["data"]["di"]) == 16 and len(io["data"]["ai_v"]) == 8


async def test_settings_persist(rig, tmp_path):
    _, client = await client_for(rig, tmp_path)
    async with client:
        ws = await client.ws_connect("/ws")
        await ws.send_str(json.dumps({"v": 1, "type": "settings", "id": "s1", "set": {"theme": "dark", "idle_min": 5}}))
        assert (await recv_type(ws, "ack"))["ok"]
    saved = json.loads((tmp_path / "s.json").read_text())
    assert saved["theme"] == "dark" and saved["idle_min"] == 5


async def test_serves_ui(rig, tmp_path):
    _, client = await client_for(rig, tmp_path)
    async with client:
        r = await client.get("/")
        assert r.status == 200 and "Cali Camping Unit" in await r.text()
        r = await client.get("/i18n/de.json")
        assert r.status == 200


def test_ui_has_no_external_resources():
    for name in ("index.html", "app.js", "style.css"):
        text = (ROOT / "ui" / name).read_text()
        assert "http://" not in text.replace("http://localhost", "") and "https://" not in text, name


def test_ui_keys_exist_in_language_file():
    import re

    lang = json.loads((ROOT / "ui" / "i18n" / "de.json").read_text())
    html = (ROOT / "ui" / "index.html").read_text()
    js = (ROOT / "ui" / "app.js").read_text()
    keys = set(re.findall(r'data-t(?:-aria)?="([^"]+)"', html)) | set(re.findall(r"\bt\('([a-z_.0-9]+)'", js))
    missing = sorted(k for k in keys if not k.endswith(".") and k not in lang)  # "x." = dynamic prefix
    assert not missing, missing
