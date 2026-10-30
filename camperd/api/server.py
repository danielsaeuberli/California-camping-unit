"""HTTP (static UI) and WebSocket ``/ws`` (SPEC 4)."""

from __future__ import annotations

import asyncio
import contextlib
import itertools
import json
import logging
from pathlib import Path
from typing import Any

from aiohttp import WSMsgType, web

from camperd import __version__
from camperd.api.store import EventLog, Settings
from camperd.clock import Clock
from camperd.core import Controller
from camperd.hal.sim import SimHal

log = logging.getLogger(__name__)

PROTO = 1
STATE_MAX_INTERVAL_S = 1.0
IO_INTERVAL_S = 0.1


class Client:
    _ids = itertools.count(1)

    def __init__(self, ws: web.WebSocketResponse) -> None:
        self.id = next(self._ids)
        self.ws = ws
        self.topics: set[str] = set()

    async def send(self, msg: dict[str, Any]) -> None:
        if not self.ws.closed:
            await self.ws.send_str(json.dumps(msg, separators=(",", ":")))


class Api:
    def __init__(
        self,
        ctrl: Controller,
        clock: Clock,
        settings: Settings,
        events: EventLog,
        ui_dir: Path,
    ) -> None:
        self.ctrl = ctrl
        self.clock = clock
        self.settings = settings
        self.events = events
        self.ui_dir = ui_dir
        self.clients: dict[int, Client] = {}
        self.roof_holder: int | None = None
        self._last_state = ""
        self._last_state_t = 0.0
        self._last_alerts: set[str] = set()
        self._task: asyncio.Task[None] | None = None

    # ----- app -----
    def app(self) -> web.Application:
        app = web.Application()
        app.router.add_get("/ws", self.ws_handler)
        app.router.add_get("/", self.index)
        if isinstance(self.ctrl.hal, SimHal):
            app.router.add_get("/sim", self.sim_page)
            app.router.add_get("/api/sim", self.sim_get)
            app.router.add_post("/api/sim", self.sim_post)
        app.router.add_static("/", self.ui_dir, show_index=False)
        app.on_startup.append(self._on_startup)
        app.on_cleanup.append(self._on_cleanup)
        return app

    async def _on_startup(self, _app: web.Application) -> None:
        self._task = asyncio.create_task(self._broadcast_loop())

    async def _on_cleanup(self, _app: web.Application) -> None:
        if self._task:
            self._task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._task

    async def index(self, _req: web.Request) -> web.StreamResponse:
        return web.FileResponse(self.ui_dir / "index.html", headers={"Cache-Control": "no-store"})

    # ----- websocket -----
    async def ws_handler(self, req: web.Request) -> web.StreamResponse:
        ws = web.WebSocketResponse(heartbeat=2.0)
        await ws.prepare(req)
        if not self.ctrl.ready:  # S1: no messages before outputs are initialised
            await ws.close()
            return ws
        client = Client(ws)
        self.clients[client.id] = client
        log.info("ui client %d connected", client.id)
        try:
            await client.send(self.hello())
            await client.send(self.state_msg())
            await client.send(self.victron_msg())
            async for msg in ws:
                if msg.type == WSMsgType.TEXT:
                    await self.handle(client, msg.data)
                elif msg.type == WSMsgType.ERROR:
                    break
        finally:
            self.clients.pop(client.id, None)
            if self.roof_holder == client.id:
                self.ctrl.roof_release()  # socket loss = missing hold messages (S3)
                self.roof_holder = None
            log.info("ui client %d disconnected", client.id)
        return ws

    async def handle(self, client: Client, raw: str) -> None:
        try:
            msg = json.loads(raw)
            if not isinstance(msg, dict) or msg.get("v") != PROTO:
                raise ValueError("bad message")
        except ValueError:
            await client.send({"v": PROTO, "type": "error", "reason": "bad_message"})
            return
        kind = msg.get("type")
        if kind == "hold":
            await self._hold(client, msg)
        elif kind == "release":
            if self.roof_holder == client.id:
                self.ctrl.roof_release()
                self.roof_holder = None
        elif kind == "cmd":
            target, values = msg.get("target"), msg.get("set")
            reason = (
                self.ctrl.command(str(target), values) if isinstance(values, dict) else "bad_message"
            )
            self.events.write("cmd", target=target, set=values, ok=reason is None, reason=reason)
            await client.send({"v": PROTO, "type": "ack", "id": msg.get("id"), "ok": reason is None, "reason": reason})
            await self.push_state(force=True)
        elif kind == "subscribe":
            topics = msg.get("topics") or []
            client.topics = {t for t in topics if t in ("io",)}
        elif kind == "settings":
            try:
                self.settings.update(msg.get("set") or {})
                ok, reason = True, None
            except (ValueError, TypeError) as exc:
                ok, reason = False, str(exc)
            await client.send({"v": PROTO, "type": "ack", "id": msg.get("id"), "ok": ok, "reason": reason})
            await self.push_state(force=True)
        elif kind == "touch":
            self.ctrl.power.user_input()
        else:
            await client.send({"v": PROTO, "type": "error", "reason": "unknown_type"})

    async def _hold(self, client: Client, msg: dict[str, Any]) -> None:
        if self.roof_holder not in (None, client.id) and self.roof_holder in self.clients:
            reason: str | None = "busy"
        else:
            direction = msg.get("dir") if msg.get("target") == "roof" else None
            reason = self.ctrl.roof_hold(str(direction), int(msg.get("seq", 0)))
            if reason is None:
                self.roof_holder = client.id
        if reason not in (None, "stale_seq"):
            ack = {"v": PROTO, "type": "ack", "id": f"hold-{msg.get('seq')}", "ok": False, "reason": reason}
            await client.send(ack)

    # ----- outgoing -----
    def hello(self) -> dict[str, Any]:
        return {
            "v": PROTO,
            "type": "hello",
            "data": {"version": __version__, "hal": self.ctrl.hal.name, "profile": self.ctrl.p.get("profile")},
        }

    def state_msg(self) -> dict[str, Any]:
        data = self.ctrl.state_data()
        data["settings"] = self.settings.public()
        data["ui"] = {"discard_first_touch": self.ctrl.power.wake_touch_pending}
        return {"v": PROTO, "type": "state", "ts": self.clock.wall_ms(), "data": data}

    def victron_msg(self) -> dict[str, Any]:
        return {"v": PROTO, "type": "victron", "ts": self.clock.wall_ms(), "data": self.ctrl.victron.snapshot()}

    def io_msg(self) -> dict[str, Any]:
        return {"v": PROTO, "type": "io", "ts": self.clock.wall_ms(), "data": self.ctrl.io_data()}

    async def _send_all(self, msg: dict[str, Any], topic: str | None = None) -> None:
        for c in list(self.clients.values()):
            if topic is None or topic in c.topics:
                with contextlib.suppress(ConnectionError, RuntimeError):
                    await c.send(msg)

    async def push_state(self, force: bool = False) -> None:
        msg = self.state_msg()
        key = json.dumps(msg["data"], sort_keys=True)
        now = self.clock.now()
        if force or key != self._last_state or now - self._last_state_t >= STATE_MAX_INTERVAL_S:
            self._last_state, self._last_state_t = key, now
            await self._send_all(msg)
            self.ctrl.power.wake_touch_pending = False
        alerts = {a["id"] for a in msg["data"]["alerts"]}
        for new in alerts - self._last_alerts:
            self.events.write("alert", id=new)
        for gone in self._last_alerts - alerts:
            self.events.write("alert_cleared", id=gone)
        self._last_alerts = alerts

    async def _broadcast_loop(self) -> None:
        tick = 0
        while True:
            await asyncio.sleep(0.05)  # state changes reach the UI within 200 ms
            tick += 1
            await self.push_state()
            if tick % 2 == 0 and any("io" in c.topics for c in self.clients.values()):
                await self._send_all(self.io_msg(), topic="io")
            if tick % 20 == 0:
                await self._send_all(self.victron_msg())

    # ----- simulator control (sim HAL only) -----
    async def sim_page(self, _req: web.Request) -> web.StreamResponse:
        return web.FileResponse(self.ui_dir / "sim.html", headers={"Cache-Control": "no-store"})

    async def sim_get(self, _req: web.Request) -> web.Response:
        assert isinstance(self.ctrl.hal, SimHal)
        return web.json_response(self.ctrl.hal.world.public())

    async def sim_post(self, req: web.Request) -> web.Response:
        hal = self.ctrl.hal
        assert isinstance(hal, SimHal)
        body = await req.json()
        world = hal.world.public()
        for key, value in body.items():
            if key not in world:
                return web.json_response({"error": f"unknown key {key}"}, status=400)
            setattr(hal.world, key, value)
        return web.json_response(hal.world.public())
