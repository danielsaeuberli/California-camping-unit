"""S3: roof relay drops after 300 ms without hold, on socket loss, and at max run time."""

import asyncio
import json

from aiohttp.test_utils import TestClient, TestServer

from camperd.api.server import Api
from camperd.api.store import EventLog, Settings
from camperd.modules.roof import Roof, RoofConfig, RoofInputs

IDLE = RoofInputs(fb_a=False, fb_b=False, vehicle_moving=False, di_ok=True)


def test_drops_without_hold(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.hold("open", 1)
    roof.tick(IDLE)
    assert roof.k1
    clock.advance(0.29)
    roof.tick(IDLE)
    assert roof.k1
    clock.advance(0.02)
    roof.tick(IDLE)
    assert not roof.k1 and not roof.k2


def test_max_run_time_latches_until_release(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    seq = 0
    t = 0.0
    while t < 59.9:
        seq += 1
        roof.hold("open", seq)
        roof.tick(IDLE)
        clock.advance(0.1)
        t += 0.1
    assert roof.k1
    seq += 1
    roof.hold("open", seq)
    clock.advance(0.2)
    roof.tick(IDLE)
    assert not roof.k1
    assert roof.block == "timeout"
    # Keeps holding: stays off.
    for _ in range(10):
        seq += 1
        assert roof.hold("open", seq) == "timeout"
        roof.tick(IDLE)
        assert not roof.k1
        clock.advance(0.1)
    # Release (no holds), then a new press works again.
    clock.advance(0.5)
    roof.tick(IDLE)
    seq += 1
    assert roof.hold("open", seq) is None
    roof.tick(IDLE)
    assert roof.k1


def test_stale_seq_does_not_extend_hold(sim_profile, clock):
    roof = Roof(RoofConfig.from_profile(sim_profile), clock)
    roof.hold("open", 10)
    roof.tick(IDLE)
    clock.advance(0.2)
    assert roof.hold("open", 5) == "stale_seq"
    clock.advance(0.15)
    roof.tick(IDLE)
    assert not roof.k1


async def test_websocket_loss_stops_roof(rig, tmp_path):
    api = Api(rig.ctrl, rig.clock, Settings(tmp_path / "s.json", rig.ctrl.p), EventLog(tmp_path / "e.jsonl"), tmp_path)
    async with TestClient(TestServer(api.app())) as client:
        ws = await client.ws_connect("/ws")
        await ws.send_str(json.dumps({"v": 1, "type": "hold", "target": "roof", "dir": "open", "seq": 1}))
        for _ in range(50):
            await asyncio.sleep(0.01)
            if rig.ctrl.roof.cmd == "open":
                break
        await rig.run_for(0.04)
        assert rig.k[0]
        await ws.close()
        for _ in range(50):
            await asyncio.sleep(0.01)
            if rig.ctrl.roof.cmd is None:
                break
        await rig.run_for(0.02)
        assert not rig.k[0] and not rig.k[1]
