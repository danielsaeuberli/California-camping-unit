"""Hardware HAL: two SM-H-001 cards on I2C bus 1 plus the ALIVE line to the wake MCU.

Bus access is blocking (smbus2) and runs in a worker thread; one lock serialises the
bus between the control loop and the watchdog thread (S10).
"""

from __future__ import annotations

import asyncio
import glob
import logging
import threading
from typing import Any

from camperd.hal.base import N_RELAY, Hal, HalError
from camperd.hal.layout import CARDS, PER_CARD, card_of
from camperd.hal.sm_h001 import REG_RELAY_VAL, Bus, CardError, SmH001
from camperd.profile import Profile, ProfileError

log = logging.getLogger(__name__)


class HwHal(Hal):
    name = "hw"

    def __init__(self, profile: Profile, bus: Bus | None = None, alive: Any = None) -> None:
        super().__init__()
        self.p = profile
        self._lock = threading.Lock()
        if bus is None:
            import smbus2

            bus = smbus2.SMBus(int(profile.get("buses.i2c", 1)))
        self.bus = bus
        stacks = []
        self.cards: dict[str, SmH001] = {}
        for key in CARDS:
            stack = profile.get(f"buses.{key}.stack", None)
            if not isinstance(stack, int):
                raise ProfileError(f"buses.{key}.stack must be set (0..7)")
            stacks.append(stack)
            self.cards[key] = SmH001(bus, stack, self._lock)
        if len(set(stacks)) != len(stacks):
            raise ProfileError("the two SM-H-001 cards need different stack levels")
        self._relays = [False] * N_RELAY
        self._alive = alive
        self._alive_gpio = profile.get("buses.alive_gpio", None)

    # ----- lifecycle -----
    async def start(self) -> None:
        await asyncio.to_thread(self._start_sync)

    def _start_sync(self) -> None:
        for key, card in self.cards.items():
            try:
                hw_ma, hw_mi, fw_ma, fw_mi = card.probe()
            except CardError as exc:
                raise HalError(f"{key} not found: {exc}") from exc
            log.info("%s stack %d: hw %d.%d fw %d.%d", key, card.stack, hw_ma, hw_mi, fw_ma, fw_mi)
            # S1 also at card power-up: the card restores REG_RELAY_DEFAULT before the Pi boots.
            if card.relay_default() != 0:
                log.warning("%s: relay power-up default was not 0, clearing it", key)
                card.set_relay_default(0)
            card.write_relays(0)
        if self.p.get("inputs.speed.mode") == "counter":
            card_key, local = card_of(self.p.channel_index("inputs.speed", "DI"))
            self.cards[card_key].enable_edge_count(local, rising=True, falling=True)
        self._set_alive(True)

    async def stop(self) -> None:
        self._set_alive(False)

    def _set_alive(self, on: bool) -> None:
        if self._alive is None and isinstance(self._alive_gpio, int):
            self._alive = AliveLine(self._alive_gpio)
        if self._alive is not None:
            try:
                self._alive.set(on)
            except OSError as exc:
                log.error("ALIVE GPIO: %s", exc)

    # ----- inputs -----
    async def read_di(self) -> list[bool]:
        try:
            masks = await asyncio.to_thread(lambda: [c.read_opto() for c in self.cards.values()])
        except CardError as exc:
            raise HalError(str(exc)) from exc
        return [bool(m & (1 << i)) for m in masks for i in range(PER_CARD)]

    async def read_ai(self) -> list[float | None]:
        try:
            volts = await asyncio.to_thread(lambda: [c.read_adc_v() for c in self.cards.values()])
        except CardError as exc:
            raise HalError(str(exc)) from exc
        return [v for card in volts for v in card]

    async def read_rails(self) -> list[float | None]:
        try:
            return list(await asyncio.to_thread(lambda: [c.read_rail_v() for c in self.cards.values()]))
        except CardError as exc:
            raise HalError(str(exc)) from exc

    async def read_counters(self, channels: tuple[int, ...]) -> dict[int, int]:
        def run() -> dict[int, int]:
            out = {}
            for ch in channels:
                key, local = card_of(ch)
                out[ch] = self.cards[key].read_count(local)
            return out

        try:
            return await asyncio.to_thread(run)
        except CardError as exc:
            raise HalError(str(exc)) from exc

    # ----- outputs -----
    async def write_relay(self, ch: int, on: bool) -> None:
        key, local = card_of(ch)
        try:
            await asyncio.to_thread(self.cards[key].set_relay, local, on)
        except CardError as exc:
            raise HalError(str(exc)) from exc
        self._relays[ch - 1] = on

    def all_off_sync(self) -> None:
        """Watchdog path (S10): clear every relay, even if the bus lock is held by a hung call."""
        got = self._lock.acquire(timeout=0.5)
        try:
            for card in self.cards.values():
                try:
                    card.bus.write_byte_data(card.addr, REG_RELAY_VAL, 0)
                except OSError as exc:
                    log.critical("all-off on stack %d failed: %s", card.stack, exc)
        finally:
            if got:
                self._lock.release()
        self._relays = [False] * N_RELAY


class AliveLine:
    """GPIO output via libgpiod v2; finds the Pi 5 header chip (pinctrl-rp1)."""

    def __init__(self, gpio: int) -> None:
        import gpiod
        from gpiod.line import Direction, Value

        self._value = Value
        chip = _header_chip()
        self._req = gpiod.request_lines(
            chip,
            consumer="camperd-alive",
            config={gpio: gpiod.LineSettings(direction=Direction.OUTPUT, output_value=Value.INACTIVE)},
        )
        self._gpio = gpio

    def set(self, on: bool) -> None:
        self._req.set_value(self._gpio, self._value.ACTIVE if on else self._value.INACTIVE)


def _header_chip() -> str:
    import gpiod

    for path in sorted(glob.glob("/dev/gpiochip*")):
        try:
            with gpiod.Chip(path) as chip:
                if "rp1" in chip.get_info().label:
                    return path
        except OSError:
            continue
    raise OSError("no pinctrl-rp1 GPIO chip found (not a Raspberry Pi 5?)")
