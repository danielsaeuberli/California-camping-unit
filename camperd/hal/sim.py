"""Simulator: a small model of the vehicle behind the HAL interface.

It reads its calibration from the (sim-overlaid) profile so that the signal chain
runs exactly as on hardware. The values in ``profiles/sim/`` are invented for the
simulator and must never be used on the vehicle.
"""

from __future__ import annotations

import asyncio
import contextlib
import threading
from dataclasses import dataclass, field
from typing import Any

from camperd.clock import Clock
from camperd.hal.base import N_AI, N_DI, N_GPIO, N_RELAY, Hal, HalError
from camperd.modules.energy import VictronStore
from camperd.profile import Profile, is_cal
from camperd.signals import SteinhartHart, StepCurve, build_curve, resistance_to_voltage

ROOF_TRAVEL_S = 20.0
HEATER_IGNITE_S = 6.0


@dataclass
class SimWorld:
    """Scenario knobs, also settable from the /sim page."""

    moving: bool = False
    ignition: bool = False
    dim58: bool = False
    waste_full: bool = False
    fresh_step: int = 2
    fridge_fault: int | None = None
    outside_c: float = 12.5
    fridge_c: float = 4.8
    roof_pos: float = 0.0  # 0 closed .. 1 open
    heater_running: bool = False
    di_card_fail: bool = False
    cerbo: bool = True
    scenario: str = "solar"
    overload: bool = False
    vebus_mode: int = 3
    current_limit: float = 10.0
    _heater_t: float = field(default=0.0, repr=False)

    def public(self) -> dict[str, Any]:
        return {k: v for k, v in self.__dict__.items() if not k.startswith("_")}


class SimHal(Hal):
    name = "sim"

    def __init__(self, profile: Profile, clock: Clock) -> None:
        super().__init__()
        self.p = profile
        self.clock = clock
        self.world = SimWorld()
        self.relays = [False] * N_RELAY
        self.gpio = [False] * N_GPIO
        self._speed_edges = 0.0
        self._lock = threading.Lock()
        self._last = clock.now()
        self._k1_dir = profile.get("outputs.roof_a.meaning")
        if not is_cal(self._k1_dir):
            self._k1_dir = "open"
        self._afterrun_s = float(profile.require("heater.afterrun_expected_s"))
        self._ref_v = 3.3  # card rail feeding the pull-ups

    # ----- physics -----
    def _step(self) -> None:
        now = self.clock.now()
        dt, self._last = now - self._last, now
        w = self.world
        k1, k2, k3, k4 = self.relays[:4]
        if k1 or k2:
            opening = (k1 and self._k1_dir == "open") or (k2 and self._k1_dir != "open")
            w.roof_pos = min(1.0, max(0.0, w.roof_pos + (1 if opening else -1) * dt / ROOF_TRAVEL_S))
        w.fridge_c += (-0.02 if k3 else 0.008) * dt
        if w.moving:
            self._speed_edges += 10 * dt  # 5 Hz square wave = 10 edges/s
        if k4:
            if not w.heater_running:
                w._heater_t += dt
                if w._heater_t >= HEATER_IGNITE_S:
                    w.heater_running, w._heater_t = True, 0.0
            else:
                w._heater_t = 0.0
        elif w.heater_running:
            w._heater_t += dt
            if w._heater_t >= self._afterrun_s:
                w.heater_running, w._heater_t = False, 0.0
        else:
            w._heater_t = 0.0

    # ----- HAL -----
    async def read_di(self) -> list[bool]:
        self._step()
        w = self.world
        if w.di_card_fail:
            raise HalError("sim: SM-H-001 not responding")
        di = [False] * N_DI
        t = self.clock.now()
        di[0] = w.roof_pos >= 1.0  # sim truth table: "10" open, "01" closed, "00" between
        di[1] = w.roof_pos <= 0.0
        di[2] = not _fridge_pulse_low(w.fridge_fault, t)
        di[3] = w.heater_running
        di[4] = w.scenario == "shore"
        di[5] = w.moving and (t % 0.2) < 0.1
        di[6] = w.dim58
        di[7] = True
        di[8] = w.moving
        di[9] = w.ignition or w.moving
        return di

    async def read_ai(self) -> list[float | None]:
        w = self.world
        ai: list[float | None] = [None] * N_AI
        ai[0] = self._ntc_v("fridge_temp", w.fridge_c)
        ai[1] = self._step_v("fresh_water", w.fresh_step)
        ai[2] = self._step_v("waste_water", 1 if w.waste_full else 0)
        ai[3] = self._ntc_v("outside_temp", w.outside_c)
        ai[5] = self._divided("roof_cmd_a_v", 12.4 if self.relays[0] else 0.0)
        ai[6] = self._divided("roof_cmd_b_v", 12.4 if self.relays[1] else 0.0)
        ai[7] = self._divided("battery_v", 12.8)
        return ai

    async def read_rail(self) -> float | None:
        return self._ref_v

    async def read_counters(self, channels: tuple[int, ...]) -> dict[int, int]:
        speed = self.p.channel_index("inputs.speed", "DI")
        return {speed: int(self._speed_edges)} if speed in channels else {}

    def _divided(self, name: str, volts: float) -> float | None:
        div = self.p.get(f"analog.{name}.divider")
        return volts / float(div) if is_cal(div) else None

    def _input_ohm(self, name: str) -> float | None:
        v = self.p.get("buses.sm_h001.ai_input_ohm", None)
        return float(v) if is_cal(v) else None

    def _pullup(self, name: str) -> float | None:
        v = self.p.get(f"analog.{name}.pullup_ohm")
        return float(v) if is_cal(v) else None

    def _ntc_v(self, name: str, temp_c: float) -> float | None:
        curve = build_curve(self.p.get(f"curves.{self.p.require(f'analog.{name}.curve')}", None))
        pull = self._pullup(name)
        if not isinstance(curve, SteinhartHart) or pull is None:
            return 1.2  # some voltage; the uncalibrated chain ignores it anyway
        return resistance_to_voltage(curve.ohm(temp_c), self._ref_v, pull, self._input_ohm(name))

    def _step_v(self, name: str, step: int) -> float | None:
        curve = build_curve(self.p.get(f"curves.{self.p.require(f'analog.{name}.curve')}", None))
        pull = self._pullup(name)
        if not isinstance(curve, StepCurve) or pull is None:
            return 1.0
        return resistance_to_voltage(curve.ohm[step], self._ref_v, pull, self._input_ohm(name))

    async def write_relay(self, ch: int, on: bool) -> None:
        with self._lock:
            self.relays[ch - 1] = on

    async def write_gpio(self, ch: int, on: bool) -> None:
        with self._lock:
            self.gpio[ch - 1] = on

    def all_off_sync(self) -> None:
        with self._lock:
            self.relays = [False] * N_RELAY
            self.gpio = [False] * N_GPIO


def _fridge_pulse_low(code: int | None, t: float) -> bool:
    """Secop-style pattern: ``code`` low pulses of 250 ms, 250 ms apart, every 4 s."""
    if not code:
        return False
    phase = t % 4.0
    idx, within = divmod(phase, 0.5)
    return idx < code and within < 0.25


SCENARIOS: dict[str, dict[str, Any]] = {
    "solar": {"soc": 76, "v": 13.4, "a": 3.2, "pv": 240, "charger": 0, "dc": 85, "state": 9, "active_in": 240,
              "in": None, "out": (230, 0.43, 50, 90, 100)},
    "night": {"soc": 54, "v": 12.9, "a": -21.0, "pv": 0, "charger": 0, "dc": 120, "state": 9, "active_in": 240,
              "in": None, "out": (230, 1.3, 50, 280, 300), "ttg": 14 * 3600},
    "shore": {"soc": 88, "v": 13.8, "a": 22.0, "pv": 0, "charger": 420, "dc": 110, "state": 5, "active_in": 0,
              "in": (230, 2.96, 50, 650), "out": (230, 0.87, 50, 180, 200)},
}


class SimCerbo:
    """Feeds the Victron store with scenario values once per second."""

    def __init__(self, store: VictronStore, world: SimWorld) -> None:
        self.store = store
        self.world = world
        self._task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        self.store.portal_id = "sim000000000"
        self.store.vebus_instance = "276"
        self._task = asyncio.create_task(self._run())

    async def stop(self) -> None:
        if self._task:
            self._task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._task

    async def _run(self) -> None:
        while True:
            self.feed()
            await asyncio.sleep(1.0)

    def feed(self) -> None:
        w = self.world
        self.store.connected = w.cerbo
        if not w.cerbo:
            return
        self.store.portal_id = self.store.portal_id or "sim000000000"
        self.store.vebus_instance = self.store.vebus_instance or "276"
        st = self.store
        while st.pending_writes:  # what the Cerbo would forward to the MultiPlus
            path, value = st.pending_writes.popleft()
            if path == "Mode":
                w.vebus_mode = int(value)
            elif path == "Ac/In/1/CurrentLimit":
                w.current_limit = float(value)
        s = dict(SCENARIOS[w.scenario])
        shore = s["active_in"] != 240
        if w.vebus_mode == 4 or (w.vebus_mode == 1 and not shore):
            s.update(state=0, out=(0, 0, 0, 0, 0), charger=0)  # off, or charger only without shore
        elif w.vebus_mode == 1:
            s.update(out=(0, 0, 0, 0, 0))
        st.ingest_system("system/0/Dc/Battery/Soc", s["soc"])
        st.ingest_system("system/0/Dc/Battery/Voltage", s["v"])
        st.ingest_system("system/0/Dc/Battery/Current", s["a"])
        st.ingest_system("system/0/Dc/Battery/Power", round(s["v"] * s["a"]))
        st.ingest_system("system/0/Dc/Battery/TimeToGo", s.get("ttg"))
        st.ingest_system("system/0/Dc/Pv/Power", s["pv"])
        st.ingest_system("system/0/Dc/Charger/Power", s["charger"])
        st.ingest_system("system/0/Dc/System/Power", s["dc"])
        st.ingest_vebus("State", s["state"])
        st.ingest_vebus("Mode", w.vebus_mode)
        st.ingest_vebus("Ac/In/1/CurrentLimit", w.current_limit)
        st.ingest_vebus("Ac/In/1/CurrentLimitIsAdjustable", 1)
        st.ingest_vebus("Ac/ActiveIn/ActiveInput", s["active_in"])
        for key, val in zip(("V", "I", "F", "P"), s["in"] or (None,) * 4, strict=True):
            st.ingest_vebus(f"Ac/ActiveIn/L1/{key}", val)
        for key, val in zip(("V", "I", "F", "P", "S"), s["out"], strict=True):
            st.ingest_vebus(f"Ac/Out/L1/{key}", val)
        st.ingest_vebus("Alarms/L1/Overload", 2 if w.overload else 0)
        for path in ("Alarms/LowBattery", "Alarms/HighTemperature", "Alarms/L1/Ripple"):
            st.ingest_vebus(path, 0)
