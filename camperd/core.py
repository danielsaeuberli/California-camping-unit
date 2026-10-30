"""Controller: hal -> signals -> modules -> outputs, one cycle every 20 ms (SPEC 3.2)."""

from __future__ import annotations

import asyncio
import logging
from collections.abc import Callable
from typing import Any

from camperd.clock import Clock
from camperd.hal.base import N_AI, N_DI, N_RELAY, Hal, HalError
from camperd.hal.layout import CARDS, card_index
from camperd.modules.energy import VictronStore
from camperd.modules.fridge import Fridge, FridgeConfig
from camperd.modules.heater import Heater, HeaterConfig
from camperd.modules.roof import Roof, RoofConfig, RoofInputs
from camperd.modules.sensors import Tanks, Thermometer, Vehicle
from camperd.power import PowerConfig, PowerInputs, PowerManager
from camperd.profile import Profile
from camperd.signals import Debouncer, PulseDecoder

log = logging.getLogger(__name__)

DI_PERIOD_S = 0.02
AI_EVERY = 5  # AI every 5th cycle = 100 ms
DI_FAIL_LIMIT = 3  # S5


class Controller:
    def __init__(self, profile: Profile, hal: Hal, clock: Clock, victron: VictronStore) -> None:
        self.p = profile
        self.hal = hal
        self.clock = clock
        self.victron = victron
        self.roof = Roof(RoofConfig.from_profile(profile), clock)
        self.fridge = Fridge(FridgeConfig.from_profile(profile), clock)
        self.heater = Heater(HeaterConfig.from_profile(profile), clock)
        self.tanks = Tanks(profile)
        self.fridge_temp = Thermometer(profile, "fridge_temp")
        self.outside_temp = Thermometer(profile, "outside_temp")
        self.vehicle = Vehicle(profile)
        self.power = PowerManager(PowerConfig.from_profile(profile), clock)

        ch = profile.channel_index
        self.ch_roof_a = ch("inputs.roof_fb_a", "DI")
        self.ch_roof_b = ch("inputs.roof_fb_b", "DI")
        self.ch_fdiag = ch("inputs.fridge_diag", "DI")
        self.ch_heat_run = ch("inputs.heater_run", "DI")
        self.k_roof_a = ch("outputs.roof_a", "K")
        self.k_roof_b = ch("outputs.roof_b", "K")
        self.k_fridge = ch("outputs.fridge_req", "K")
        self.k_heater = ch("outputs.heater_splus", "K")
        # Channels fed through a divider from 12 V: must stay below the card's ai_max_v.
        self.divided_ai = [
            profile.channel_index(f"analog.{n}", "AI")
            for n, spec in profile.require("analog").items()
            if "divider" in spec
        ]
        self.ai_max_v = [float(profile.get(f"buses.{c}.ai_max_v", 3.0)) for c in CARDS]
        self.rails: list[float | None] = [None] * len(CARDS)
        self.ai_overvoltage: list[int] = []
        self.ch_speed = ch("inputs.speed", "DI")
        self.speed_counter = profile.get("inputs.speed.mode") == "counter"
        self.outside_source = profile.get("outside_temp_source", "g249")

        self._deb = {
            c: Debouncer(float(profile.get(f"inputs.{n}.debounce_ms", 0)) / 1000)
            for c, n in ((self.ch_roof_a, "roof_fb_a"), (self.ch_roof_b, "roof_fb_b"), (self.ch_heat_run, "heater_run"))
        }
        fd = "inputs.fridge_diag"
        self.pulse = PulseDecoder(
            pulse_s=float(profile.require(f"{fd}.pulse_ms")) / 1000,
            frame_s=float(profile.require(f"{fd}.frame_ms")) / 1000,
            active_low=profile.get(f"{fd}.active", "low") == "low",
        )

        self.di: list[bool | None] = [None] * N_DI
        self.ai: list[float | None] = [None] * N_AI
        self.di_ok = False
        self._di_errors = 0
        self.k = [False] * N_RELAY
        self.h = [False] * len(self.heater.cfg.ladder_channels)  # ladder bits (relays K9-K12)
        self._written: dict[int, bool] = {}
        self.counts: dict[int, int] = {}
        self._cycle = 0
        self.ready = False
        self.on_cycle: list[Callable[[], None]] = []
        self.shutdown_requested = False

    # ----- S1 -----
    async def init_outputs(self) -> None:
        """All K and H outputs off before any message is accepted."""
        await self.hal.all_off()
        for i in range(1, N_RELAY + 1):
            self._written[i] = False
        self.ready = True

    # ----- commands (from the API) -----
    def command(self, target: str, values: dict[str, Any]) -> str | None:
        """Returns a reject reason or None."""
        self.power.user_input()
        if target == "fridge":
            return self.fridge.set(on=values.get("on"), target_c=values.get("target_c"))
        if target == "heater":
            return self.heater.set(on=values.get("on"), level=values.get("level"))
        return "unknown_target"

    def roof_hold(self, direction: str, seq: int) -> str | None:
        self.power.user_input()
        return self.roof.hold(direction, seq)

    def roof_release(self) -> None:
        self.roof.release()

    # ----- cycle -----
    async def cycle(self) -> None:
        now = self.clock.now()
        try:
            raw_di = await self.hal.read_di()
            self._di_errors = 0
            self.di = [bool(v) for v in raw_di]
            self.di_ok = True
        except HalError as exc:
            self._di_errors += 1
            if self._di_errors >= DI_FAIL_LIMIT:
                if self.di_ok:
                    log.error("DI card failed %d times: %s", self._di_errors, exc)
                self.di_ok = False
                self.di = [None] * N_DI  # stale
        if self.speed_counter and self.di_ok:
            try:
                self.counts = await self.hal.read_counters((self.ch_speed,))
            except HalError:
                self.counts = {}
        if self._cycle % AI_EVERY == 0:
            try:
                self.ai = await self.hal.read_ai()
                self.rails = await self.hal.read_rails()
            except HalError as exc:
                log.warning("AI read failed: %s", exc)
                self.ai = [None] * N_AI
                self.rails = [None] * len(CARDS)
            self.ai_overvoltage = [
                c for c in self.divided_ai
                if (v := self.ai[c - 1]) is not None and v > self.ai_max_v[card_index(c)]
            ]
        self._cycle += 1

        self.vehicle.update(self.di, now, self.counts)
        self.fridge_temp.update(self.ai, self.rails, now)
        self.outside_temp.update(self.ai, self.rails, now)
        self.tanks.update(self.ai, self.rails, self.vehicle.moving, now)

        def deb(ch: int) -> bool | None:
            raw = self.di[ch - 1]
            return None if raw is None else self._deb[ch].update(raw, now)

        fdiag = self.di[self.ch_fdiag - 1]
        code = self.pulse.update(fdiag, now) if fdiag is not None else None

        k1, k2 = self.roof.tick(
            RoofInputs(
                fb_a=deb(self.ch_roof_a),
                fb_b=deb(self.ch_roof_b),
                vehicle_moving=self.vehicle.moving,
                di_ok=self.di_ok,
                ignition=self.vehicle.ignition,
            )
        )
        k3 = self.fridge.tick(self.fridge_temp.value, code)
        k4, h = self.heater.tick(deb(self.ch_heat_run))

        relays = {self.k_roof_a: k1, self.k_roof_b: k2, self.k_fridge: k3, self.k_heater: k4}
        relays.update(zip(self.heater.cfg.ladder_channels, h, strict=True))
        self.k = [relays.get(i, False) for i in range(1, N_RELAY + 1)]
        self.h = h
        await self._write_outputs()

        mode = self.power.tick(
            PowerInputs(
                ignition=self.vehicle.ignition,
                fridge_active=self.fridge.thermostat_active(),
                heater_phase=self.heater.phase,
                roof_moving=self.roof.moving,
                battery_v=self.victron.battery_v(),
            )
        )
        if mode == "shutdown" and not self.shutdown_requested:
            # OPEN: M6 - send SHUTDOWN to the wake MCU and power off. For now only logged.
            self.shutdown_requested = True
            log.warning("shutdown conditions met (power-off not wired up before M6)")
        for cb in self.on_cycle:
            cb()

    async def _write_outputs(self) -> None:
        # Relays that switch off go first, so a reversal never overlaps (S2).
        writes = sorted(enumerate(self.k, start=1), key=lambda w: w[1])
        for ch, val in writes:
            if self._written.get(ch) != val:
                await self.hal.write_relay(ch, val)
                self._written[ch] = val

    async def run(self, stop: asyncio.Event, kick: Callable[[], None] = lambda: None) -> None:
        if not self.ready:
            await self.init_outputs()
        loop = asyncio.get_running_loop()
        next_t = loop.time()
        while not stop.is_set():
            await self.cycle()
            kick()
            next_t += DI_PERIOD_S
            delay = next_t - loop.time()
            if delay < -0.5:
                next_t = loop.time()  # fell behind, resync
            await asyncio.sleep(max(0.0, delay))
        await self.hal.all_off()

    # ----- messages -----
    def outside_c(self) -> float | None:
        if self.outside_source == "cerbo":
            return None  # OPEN: no Cerbo temperature path defined yet
        v = self.outside_temp.value
        return round(v.value, 1) if v.ok else None

    def alerts(self) -> list[dict[str, str]]:
        out = self.roof.alerts() + self.fridge.alerts() + self.heater.alerts() + self.tanks.alerts()
        if not self.di_ok and self.ready:
            out.append({"id": "di_card", "level": "err"})
        if self.ai_overvoltage:
            out.append({"id": "ai_overvoltage", "level": "err"})
        if self.power.undervoltage:
            out.append({"id": "undervoltage", "level": "err"})
        return out

    def state_data(self) -> dict[str, Any]:
        return {
            "power": {"mode": self.power.mode, "ignition": self.vehicle.ignition},
            "roof": self.roof.state(),
            "fridge": self.fridge.state(),
            "heater": self.heater.state(),
            "tanks": self.tanks.state(),
            "outside_c": self.outside_c(),
            "outside_q": self.outside_temp.value.q.value,
            "vehicle": self.vehicle.state(),
            "alerts": self.alerts(),
        }

    def io_data(self) -> dict[str, Any]:
        return {
            "di": [None if v is None else int(v) for v in self.di],
            "k": [int(v) for v in self.k],
            "h": [int(v) for v in self.h],
            "ai_v": [None if v is None else round(v, 3) for v in self.ai],
            "rails_v": [None if v is None else round(v, 3) for v in self.rails],
            "ladder_ohm": None if (ohm := self.heater.ladder_ohm()) is None else round(ohm, 1),
        }
