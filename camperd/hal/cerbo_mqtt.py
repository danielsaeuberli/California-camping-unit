"""MQTT client for the Victron Cerbo GX (SPEC 5).

Venus OS publishes ``N/<portal>/<service>/<instance>/<path>`` with a JSON payload
``{"value": ...}`` while a keepalive is sent to ``R/<portal>/keepalive``
(dbus-flashmq: https://github.com/victronenergy/dbus-flashmq#keepalive).
Writes (``W/...``) are limited to the VE.Bus paths in
:data:`camperd.modules.victron_control.WRITABLE`, queued by :class:`VictronControl`.
"""

from __future__ import annotations

import asyncio
import contextlib
import json
import logging
from typing import Any

from camperd.modules.energy import VictronStore
from camperd.modules.victron_control import WRITABLE

log = logging.getLogger(__name__)

KEEPALIVE_PAYLOAD = json.dumps({"keepalive-options": ["suppress-republish"]})


def write_topic(portal: str, instance: str, path: str) -> str:
    """The only W/ topics this client may publish (SPEC 5.1)."""
    if path not in WRITABLE:
        raise ValueError(f"write to vebus path {path!r} is not allowed")
    return f"W/{portal}/vebus/{instance}/{path}"


def parse(topic: str, payload: bytes) -> tuple[str, str, str, str, Any] | None:
    """``N/<portal>/<service>/<instance>/<path>`` -> (portal, service, instance, path, value)."""
    parts = topic.split("/", 4)
    if len(parts) < 5 or parts[0] != "N":
        return None
    try:
        data = json.loads(payload) if payload else {}
    except ValueError:
        return None
    value = data.get("value") if isinstance(data, dict) else None
    return parts[1], parts[2], parts[3], parts[4], value


class CerboClient:
    def __init__(self, store: VictronStore, host: str, port: int = 1883, keepalive_s: float = 30.0) -> None:
        self.store = store
        self.host = host
        self.port = port
        self.keepalive_s = keepalive_s
        self._task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        self._task = asyncio.create_task(self._run())

    async def stop(self) -> None:
        if self._task:
            self._task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._task

    def handle(self, topic: str, payload: bytes) -> list[str]:
        """Feeds one message into the store; returns topics to subscribe to next."""
        parsed = parse(topic, payload)
        if parsed is None:
            return []
        portal, service, instance, path, value = parsed
        follow: list[str] = []
        if self.store.portal_id is None and service == "system" and path == "Serial":
            self.store.portal_id = portal
            log.info("Cerbo portal id %s", portal)
            follow += [f"N/{portal}/system/0/Dc/#", f"N/{portal}/vebus/+/#"]
        if portal != self.store.portal_id:
            return follow
        if service == "system" and instance == "0":
            self.store.ingest_system(f"system/0/{path}", value)
        elif service == "vebus":
            if self.store.vebus_instance is None and path == "ProductId":
                self.store.vebus_instance = instance
                log.info("VE.Bus instance %s", instance)
            if instance == self.store.vebus_instance:
                self.store.ingest_vebus(path, value)
        return follow

    async def _run(self) -> None:
        import aiomqtt

        backoff = 1.0
        while True:
            try:
                async with aiomqtt.Client(self.host, self.port, identifier="camperd") as client:
                    self.store.connected = True
                    backoff = 1.0
                    await client.subscribe("N/+/system/0/Serial")
                    ka = asyncio.create_task(self._keepalive(client))
                    wr = asyncio.create_task(self._writer(client))
                    try:
                        async for msg in client.messages:
                            raw = msg.payload if isinstance(msg.payload, (bytes, bytearray)) else b""
                            for topic in self.handle(str(msg.topic), bytes(raw)):
                                await client.subscribe(topic)
                    finally:
                        ka.cancel()
                        wr.cancel()
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                log.warning("Cerbo MQTT: %s (retry in %.0f s)", exc, backoff)
            self.store.connected = False
            await asyncio.sleep(backoff)
            backoff = min(30.0, backoff * 2)

    async def _writer(self, client: Any) -> None:
        while True:
            portal, inst = self.store.portal_id, self.store.vebus_instance
            while self.store.pending_writes and portal and inst:
                path, value = self.store.pending_writes.popleft()
                topic = write_topic(portal, inst, path)
                log.info("Cerbo write %s = %s", topic, value)
                await client.publish(topic, json.dumps({"value": value}))
            await asyncio.sleep(0.1)

    async def _keepalive(self, client: Any) -> None:
        while True:
            if self.store.portal_id:
                topic = f"R/{self.store.portal_id}/keepalive"
                await client.publish(topic, KEEPALIVE_PAYLOAD)
            await asyncio.sleep(self.keepalive_s if self.store.portal_id else 1.0)
