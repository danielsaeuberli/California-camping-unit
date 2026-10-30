"""Hardware HAL: SM-H-001 (relays, analog), SM-I-009 (digital inputs) on I2C bus 1,
Purecrea relay module (heater ladder H1-H4) and the ALIVE line on Pi GPIOs.

Bus access is blocking (smbus2) and runs in a worker thread; one lock serialises the
bus between the control loop and the watchdog thread (S10).
"""

from __future__ import annotations

import asyncio
import glob
import logging
import threading
from typing import Any, Protocol

from camperd.hal.base import N_GPIO, N_RELAY, Hal, HalError
from camperd.hal.sm_h001 import REG_RELAY_VAL, Bus, CardError, SmH001
from camperd.hal.sm_i009 import SmI009
from camperd.profile import Profile, ProfileError, is_cal

log = logging.getLogger(__name__)


class Lines(Protocol):
    """GPIO outputs by BCM number (lets tests inject a fake)."""

    def set(self, gpio: int, level: bool) -> None: ...


class HwHal(Hal):
    name = "hw"

    def __init__(self, profile: Profile, bus: Bus | None = None, lines: Lines | None = None) -> None:
        super().__init__()
        self.p = profile
        self._lock = threading.Lock()
        if bus is None:
            import smbus2

            bus = smbus2.SMBus(int(profile.get("buses.i2c", 1)))
        self.bus = bus
        self.out = SmH001(bus, _stack(profile, "sm_h001"), self._lock)
        self.inp = SmI009(bus, _stack(profile, "sm_i009"), self._lock)

        segs = profile.require("outputs.heater_ladder.segments")
        self.ladder_gpio = [int(s["gpio"]) for s in segs]
        if len(self.ladder_gpio) != N_GPIO:
            raise ProfileError(f"outputs.heater_ladder.segments: expected {N_GPIO} entries")
        active = profile.get("outputs.heater_ladder.gpio_active")
        # OPEN: polarity of the Purecrea inputs (TODO_CHECK) - unknown -> never energise H1-H4
        self.ladder_active_high: bool | None = (active == "high") if is_cal(active) else None
        alive = profile.get("buses.alive_gpio", None)
        self.alive_gpio = int(alive) if isinstance(alive, int) else None
        self._lines = lines

    # ----- lifecycle -----
    async def start(self) -> None:
        await asyncio.to_thread(self._start_sync)

    def _start_sync(self) -> None:
        try:
            hw_ma, hw_mi, fw_ma, fw_mi = self.out.probe()
        except CardError as exc:
            raise HalError(f"SM-H-001 not found: {exc}") from exc
        log.info("SM-H-001 stack %d: hw %d.%d fw %d.%d", self.out.stack, hw_ma, hw_mi, fw_ma, fw_mi)
        # S1 also at card power-up: the card restores REG_RELAY_DEFAULT before the Pi boots.
        if self.out.relay_default() != 0:
            log.warning("SM-H-001: relay power-up default was not 0, clearing it")
            self.out.set_relay_default(0)
        self.out.write_relays(0)

        try:
            rev = self.inp.probe()
        except CardError as exc:
            raise HalError(f"SM-I-009 not found: {exc}") from exc
        log.info("SM-I-009 stack %d: %s", self.inp.stack, f"hw {rev[0]}.{rev[1]} fw {rev[2]}.{rev[3]}" if rev else "V2")
        if self.p.get("inputs.speed.mode") == "counter":
            if self.inp.has_counters:
                self.inp.enable_edge_count(self.p.channel_index("inputs.speed", "DI"))
            else:
                log.warning("SM-I-009 has no edge counters (V2): speed falls back to 20 ms polling")

        if self._lines is None:
            self._lines = GpioLines(self._requested_lines())
        self._write_ladder_all(False)
        if self.alive_gpio is not None:
            self._lines.set(self.alive_gpio, True)

    def _requested_lines(self) -> dict[int, bool]:
        """GPIO -> initial level. Ladder lines only if their polarity is known."""
        init: dict[int, bool] = {}
        if self.ladder_active_high is not None:
            init.update({g: not self.ladder_active_high for g in self.ladder_gpio})
        if self.alive_gpio is not None:
            init[self.alive_gpio] = False
        return init

    async def stop(self) -> None:
        if self._lines is not None and self.alive_gpio is not None:
            try:
                self._lines.set(self.alive_gpio, False)
            except OSError as exc:
                log.error("ALIVE GPIO: %s", exc)

    # ----- inputs -----
    async def read_di(self) -> list[bool]:
        try:
            return await asyncio.to_thread(self.inp.read_inputs)
        except CardError as exc:
            raise HalError(str(exc)) from exc

    async def read_ai(self) -> list[float | None]:
        try:
            return list(await asyncio.to_thread(self.out.read_adc_v))
        except CardError as exc:
            raise HalError(str(exc)) from exc

    async def read_rail(self) -> float | None:
        try:
            return await asyncio.to_thread(self.out.read_rail_v)
        except CardError as exc:
            raise HalError(str(exc)) from exc

    async def read_counters(self, channels: tuple[int, ...]) -> dict[int, int]:
        if not self.inp.has_counters:
            return {}
        try:
            return await asyncio.to_thread(lambda: {ch: self.inp.read_count(ch) for ch in channels})
        except CardError as exc:
            raise HalError(str(exc)) from exc

    # ----- outputs -----
    async def write_relay(self, ch: int, on: bool) -> None:
        if not 1 <= ch <= N_RELAY:
            raise ValueError(f"relay K{ch} does not exist")
        try:
            await asyncio.to_thread(self.out.set_relay, ch, on)
        except CardError as exc:
            raise HalError(str(exc)) from exc

    async def write_gpio(self, ch: int, on: bool) -> None:
        if on and self.ladder_active_high is None:
            raise HalError("heater ladder polarity (outputs.heater_ladder.gpio_active) not checked")
        await asyncio.to_thread(self._write_ladder, ch, on)

    def _write_ladder(self, ch: int, on: bool) -> None:
        if self.ladder_active_high is None or self._lines is None:
            return  # lines not requested: they keep the boot level from config.txt
        try:
            self._lines.set(self.ladder_gpio[ch - 1], on == self.ladder_active_high)
        except OSError as exc:
            raise HalError(f"GPIO{self.ladder_gpio[ch - 1]}: {exc}") from exc

    def _write_ladder_all(self, on: bool) -> None:
        for ch in range(1, N_GPIO + 1):
            self._write_ladder(ch, on)

    def all_off_sync(self) -> None:
        """Watchdog path (S10): clear every output, even if the bus lock is held by a hung call."""
        got = self._lock.acquire(timeout=0.5)
        try:
            try:
                self.out.bus.write_byte_data(self.out.addr, REG_RELAY_VAL, 0)
            except OSError as exc:
                log.critical("all-off on SM-H-001 failed: %s", exc)
        finally:
            if got:
                self._lock.release()
        try:
            self._write_ladder_all(False)
        except HalError as exc:
            log.critical("all-off on the ladder GPIOs failed: %s", exc)


def _stack(p: Profile, key: str) -> int:
    stack = p.get(f"buses.{key}.stack", None)
    if not isinstance(stack, int) or not 0 <= stack <= 7:
        raise ProfileError(f"buses.{key}.stack must be set (0..7)")
    return stack


class GpioLines:
    """GPIO outputs via libgpiod v2 on the Pi 5 header chip (pinctrl-rp1)."""

    def __init__(self, initial: dict[int, bool]) -> None:
        import gpiod
        from gpiod.line import Direction, Value

        self._value = Value
        self._req: Any = None
        if initial:
            self._req = gpiod.request_lines(
                _header_chip(),
                consumer="camperd",
                config={
                    g: gpiod.LineSettings(
                        direction=Direction.OUTPUT, output_value=Value.ACTIVE if v else Value.INACTIVE
                    )
                    for g, v in initial.items()
                },
            )

    def set(self, gpio: int, level: bool) -> None:
        if self._req is None:
            raise OSError(f"GPIO{gpio} not requested")
        self._req.set_value(gpio, self._value.ACTIVE if level else self._value.INACTIVE)


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
