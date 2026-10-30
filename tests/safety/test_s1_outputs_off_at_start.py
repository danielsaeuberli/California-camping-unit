"""S1: K1-K4 off and H1-H4 unpowered before camperd accepts any message."""

from camperd.clock import FakeClock
from camperd.core import Controller
from camperd.hal.sim import SimHal
from camperd.modules.energy import VictronStore


async def test_init_switches_everything_off(sim_profile):
    clock = FakeClock()
    hal = SimHal(sim_profile, clock)
    hal.relays = [True] * 16  # e.g. left on by a crashed previous run
    ctrl = Controller(sim_profile, hal, clock, VictronStore(clock))
    assert not ctrl.ready
    await ctrl.init_outputs()
    assert ctrl.ready
    assert hal.relays == [False] * 16


async def test_ladder_unpowered_means_base_resistance(rig):
    await rig.run_for(0.5)
    assert rig.h == [False] * 4
    assert rig.ctrl.heater.ladder_ohm() == rig.ctrl.heater.cfg.base_ohm


async def test_uncalibrated_ladder_never_switches(profile, clock):
    from tests.conftest import Rig

    r = Rig(profile, clock)
    await r.ctrl.init_outputs()
    assert r.ctrl.command("heater", {"level": 15}) == "ladder_uncalibrated"
    r.ctrl.command("heater", {"on": True})
    await r.run_for(1.0)
    assert r.h == [False] * 4
    assert r.ctrl.heater.ladder_ohm() is None


async def test_api_refuses_clients_before_init(sim_profile, tmp_path):
    from aiohttp.test_utils import TestClient, TestServer

    from camperd.api.server import Api
    from camperd.api.store import EventLog, Settings

    clock = FakeClock()
    hal = SimHal(sim_profile, clock)
    ctrl = Controller(sim_profile, hal, clock, VictronStore(clock))
    api = Api(ctrl, clock, Settings(tmp_path / "s.json", sim_profile), EventLog(tmp_path / "e.jsonl"), tmp_path)
    async with TestClient(TestServer(api.app())) as client:
        ws = await client.ws_connect("/ws")
        msg = await ws.receive()
        assert msg.type.name in ("CLOSE", "CLOSED", "CLOSING")
