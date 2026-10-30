"""Read-only modules: tanks, climate (NTC temperatures) and vehicle signals."""

from __future__ import annotations

from typing import Any

from camperd.profile import Profile, ProfileError, is_cal
from camperd.signals import (
    CounterWindow,
    Debouncer,
    EdgeCounter,
    MedianFilter,
    Quality,
    SteinhartHart,
    StepCurve,
    Value,
    build_curve,
    classify_resistance,
    sensor_resistance,
)


class ResistiveInput:
    """AI voltage -> sensor resistance, ratiometric against the pull-up rail (``analog.ref_5v``).

    Reference is the measured 3.3 V rail of the SM-H-001; corrects for the AI input
    impedance when known. A voltage above the rail means an external source -> implausible.
    """

    def __init__(self, p: Profile, name: str) -> None:
        self.name = name
        self.channel = p.channel_index(f"analog.{name}", "AI")
        pullup = p.get(f"analog.{name}.pullup_ohm")
        self.pullup = float(pullup) if is_cal(pullup) else None
        source = p.get("analog.pullup_ref.source", None)
        if source != "card_rail":
            raise ProfileError("analog.pullup_ref.source: only 'card_rail' is supported")
        input_ohm = p.get("buses.sm_h001.ai_input_ohm", None)
        self.input_ohm = float(input_ohm) if is_cal(input_ohm) else None
        self.open_ohm = float(p.require("curves.open_ohm"))
        self.short_ohm = float(p.require("curves.short_ohm"))
        self.curve = build_curve(p.get(f"curves.{p.require(f'analog.{name}.curve')}", None))

    def resistance(self, ai: list[float | None], v_ref: float | None, now: float) -> Value:
        v = ai[self.channel - 1]
        if self.pullup is None or self.curve is None:
            return Value(None, now, Quality.UNCALIBRATED)
        if v is None or v_ref is None:
            return Value(None, now, Quality.STALE)
        if v > v_ref + 0.05:
            return Value(None, now, Quality.IMPLAUSIBLE)  # above the pull-up rail: external voltage
        ohm = sensor_resistance(v, v_ref, self.pullup, self.input_ohm)
        q = classify_resistance(ohm, self.open_ohm, self.short_ohm)
        return Value(ohm if q is Quality.OK else None, now, q)


class Thermometer:
    def __init__(self, p: Profile, name: str) -> None:
        self.input = ResistiveInput(p, name)
        self.value = Value(None, 0.0, Quality.UNCALIBRATED)

    def update(self, ai: list[float | None], rail: float | None, now: float) -> Value:
        r = self.input.resistance(ai, rail, now)
        curve = self.input.curve
        if not r.ok or not isinstance(curve, SteinhartHart):
            self.value = r
        else:
            t = curve.temp_c(r.value)
            q = Quality.OK if -40 <= t <= 90 else Quality.IMPLAUSIBLE
            self.value = Value(t if q is Quality.OK else None, now, q)
        return self.value


class Tank:
    """Discrete level; median filter, new level only after ``stable_s``."""

    def __init__(self, p: Profile, name: str) -> None:
        self.input = ResistiveInput(p, name)
        self.median_s = float(p.require("tanks.median_s"))
        self.median_moving_s = float(p.require("tanks.median_moving_s"))
        self.stable_s = float(p.require("tanks.stable_s"))
        self._filter = MedianFilter(self.median_s)
        self.step: int | None = None
        self.q = Quality.UNCALIBRATED
        self._candidate: int | None = None
        self._since = 0.0

    def update(self, ai: list[float | None], rail: float | None, moving: bool, now: float) -> None:
        r = self.input.resistance(ai, rail, now)
        curve = self.input.curve
        if not r.ok or not isinstance(curve, StepCurve):
            self.q = r.q
            self.step = None
            self._filter.reset()
            return
        self._filter.window_s = self.median_moving_s if moving else self.median_s
        ohm = self._filter.update(r.value, now)
        raw = curve.classify(ohm, self.step)
        self.q = Quality.OK
        if self.step is None:
            self.step = raw
        elif raw != self.step:
            if raw != self._candidate:
                self._candidate, self._since = raw, now
            elif now - self._since >= self.stable_s:
                self.step, self._candidate = raw, None
        else:
            self._candidate = None


class Tanks:
    def __init__(self, p: Profile) -> None:
        self.fresh = Tank(p, "fresh_water")
        self.waste = Tank(p, "waste_water")

    def update(self, ai: list[float | None], rail: float | None, moving: bool, now: float) -> None:
        self.fresh.update(ai, rail, moving, now)
        self.waste.update(ai, rail, moving, now)

    @property
    def waste_full(self) -> bool | None:
        return None if self.waste.step is None else self.waste.step == 1

    def state(self) -> dict[str, Any]:
        return {
            "fresh_step": self.fresh.step,
            "fresh_q": self.fresh.q.value,
            "waste_full": self.waste_full,
            "waste_q": self.waste.q.value,
        }

    def alerts(self) -> list[dict[str, str]]:
        return [{"id": "waste_full", "level": "warn"}] if self.waste_full else []


class Vehicle:
    """Ignition, 86s, D+, 58d, shore (log only) and moving/standing from speed edges."""

    def __init__(self, p: Profile) -> None:
        def ch(name: str) -> int:
            return p.channel_index(f"inputs.{name}", "DI")

        ign_name = _input_for_channel(p, p.require("vehicle.ignition_source"))
        dim_name = _input_for_channel(p, p.require("vehicle.illumination_source"))
        self.ch_ign = ch(ign_name)
        self.ch_dim = ch(dim_name)
        self.ch_86s = ch("term_86s")
        self.ch_dplus = ch("d_plus")
        self.ch_shore = ch("shore_a11")
        self.ch_speed = ch("speed")
        self._ign = Debouncer(float(p.get(f"inputs.{ign_name}.debounce_ms", 100)) / 1000)
        window_s = float(p.require("inputs.speed.window_ms")) / 1000
        self.speed_mode = p.get("inputs.speed.mode", "edges")
        self._speed = EdgeCounter(window_s)  # level polling; fallback without a counter
        self._speed_cnt = CounterWindow(window_s)
        self.min_edges = int(p.require("inputs.speed.moving_min_edges"))
        self.ignition = False
        self.moving = False
        self.t86s: bool | None = None
        self.d_plus: bool | None = None
        self.dim58 = False
        self._dim_window_s = float(p.get(f"inputs.{dim_name}.window_ms", 0)) / 1000
        self._dim_last_high = -1e9
        self.shore: bool | None = None

    def update(self, di: list[bool | None], now: float, counts: dict[int, int] | None = None) -> None:
        def get(c: int) -> bool | None:
            return di[c - 1]

        ign = get(self.ch_ign)
        if ign is not None:
            self.ignition = bool(self._ign.update(ign, now))
        count = counts.get(self.ch_speed) if counts else None
        speed = get(self.ch_speed)
        if self.speed_mode == "counter" and count is not None:
            self.moving = self._speed_cnt.update(count, now) >= self.min_edges
        elif speed is not None:
            self.moving = self._speed.update(speed, now) >= self.min_edges
        self.t86s = get(self.ch_86s)
        self.d_plus = get(self.ch_dplus)  # OPEN: O8 - logged only
        if get(self.ch_dim):
            self._dim_last_high = now
        # 58d may be PWM (dimmed instrument lighting): on if high anywhere in the window
        self.dim58 = now - self._dim_last_high <= self._dim_window_s
        self.shore = get(self.ch_shore)  # OPEN: O5 - logged only

    def state(self) -> dict[str, Any]:
        return {"moving": self.moving, "d_plus": self.d_plus, "t86s": self.t86s, "dim58": self.dim58}


def _input_for_channel(p: Profile, channel: str) -> str:
    for name, spec in p.require("inputs").items():
        if spec.get("channel") == channel:
            return str(name)
    raise ValueError(f"no input configured on {channel}")
