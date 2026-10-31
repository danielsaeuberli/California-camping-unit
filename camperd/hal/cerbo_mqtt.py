"""MQTT client for the Victron Cerbo GX (SPEC 5).

Venus OS publishes ``N/<portal>/<service>/<instance>/<path>`` with a JSON payload
``{"value": ...}`` only while it receives keepalives on ``R/<portal>/keepalive``
(dbus-flashmq: https://github.com/victronenergy/dbus-flashmq#keepalive). An empty
keepalive also makes Venus republish every value, so the keepalive interval must stay
well below ``stale_s``: values are otherwise only sent on change and a steady value
would look stale.

Network (SPEC 2.7): the Pi reaches the Cerbo over the Cerbo's own Wi-Fi access point,
never through the camper router. The client reconnects on its own after a Cerbo reboot
or firmware update.
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

KEEPALIVE_PAYLOAD = ""  # empty: Venus republishes all values (refreshes their age)
MQTT_KEEPALIVE_S = 10  # protocol ping: notice a dropped Wi-Fi link quickly
BACKOFF_MAX_S = 10.0  # local AP: retrying is cheap; data is back <= 10 s after the Cerbo


def write_topic(portal: str, instance: str, path: str) -> str:
    """The only W/ topics this client may publish (SPEC 5.1)."""
    if path not in WRITABLE:
        raise ValueError(f"write to vebus path {path!r} is not allowed")
    return f"W/{portal}/vebus/{instance}/{path}"


def keepalive_topic(portal: str) -> str:
    return f"R/{portal}/keepalive"


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
    def __init__(
        self,
        store: VictronStore,
        host: str,
        port: int = 1883,
        keepalive_s: float = 5.0,
        portal_id: str | None = None,
    ) -> None:
        self.store = store
        self.host = host
        self.port = port
        if keepalive_s > store.stale_s / 2:
            raise ValueError(f"buses.cerbo.keepalive_s ({keepalive_s}) must be <= stale_s / 2 ({store.stale_s / 2})")
        self.keepalive_s = keepalive_s
        self.fixed_portal = portal_id
        if portal_id:
            store.portal_id = portal_id
        self._task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        self._task = asyncio.create_task(self._run())

    async def reconfigure(self, host: str, port: int) -> None:
        """New Cerbo address from the network settings: reconnect there."""
        if (host, port) == (self.host, self.port):
            return
        log.info("Cerbo address changed to %s:%d", host, port)
        await self.stop()
        self.host, self.port = host, port
        self.store.connected = False
        await self.start()

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
            follow += self.data_topics()
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

    def data_topics(self) -> list[str]:
        portal = self.store.portal_id
        return [f"N/{portal}/system/0/Dc/#", f"N/{portal}/vebus/+/#"] if portal else []

    def on_connect(self) -> list[str]:
        """Topics to subscribe after every (re)connect, incl. after a Cerbo reboot."""
        if not self.fixed_portal:
            self.store.portal_id = None  # rediscover: a replaced Cerbo has a new portal id
        self.store.vebus_instance = None  # may change after a firmware update / VE.Bus reset
        return ["N/+/system/0/Serial", *self.data_topics()]

    async def _run(self) -> None:
        import aiomqtt

        backoff = 1.0
        while True:
            try:
                async with aiomqtt.Client(
                    self.host, self.port, identifier="camperd", keepalive=MQTT_KEEPALIVE_S, timeout=10
                ) as client:
                    self.store.connected = True
                    backoff = 1.0
                    log.info("Cerbo MQTT connected (%s:%d)", self.host, self.port)
                    for topic in self.on_connect():
                        await client.subscribe(topic)
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
            except Exception as exc:  # broker gone, Wi-Fi dropped, Cerbo rebooting ...
                log.warning("Cerbo MQTT: %s (retry in %.0f s)", exc, backoff)
            self.store.connected = False
            self.store.pending_writes.clear()  # never replay a stale user command
            await asyncio.sleep(backoff)
            backoff = min(BACKOFF_MAX_S, backoff * 2)

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
                await client.publish(keepalive_topic(self.store.portal_id), KEEPALIVE_PAYLOAD)
            await asyncio.sleep(self.keepalive_s if self.store.portal_id else 1.0)
