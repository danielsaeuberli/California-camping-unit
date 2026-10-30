"""Raw value -> value with timestamp and quality; filters."""

from __future__ import annotations

import math
import statistics
from collections import deque
from dataclasses import dataclass
from enum import StrEnum
from typing import Any

from camperd.profile import is_cal


class Quality(StrEnum):
    OK = "ok"
    STALE = "stale"
    OPEN = "open"
    SHORT = "short"
    IMPLAUSIBLE = "implausible"
    UNCALIBRATED = "uncalibrated"


@dataclass(frozen=True)
class Value:
    value: Any
    ts: float
    q: Quality = Quality.OK

    @property
    def ok(self) -> bool:
        return self.q is Quality.OK


class Debouncer:
    """A level change is accepted after it has been stable for ``debounce_s``."""

    def __init__(self, debounce_s: float, initial: bool | None = None) -> None:
        self.debounce_s = debounce_s
        self.state = initial
        self._candidate: bool | None = None
        self._since = 0.0

    def update(self, raw: bool, now: float) -> bool | None:
        if self.state is None:
            self.state = raw
            return raw
        if raw == self.state:
            self._candidate = None
        elif raw != self._candidate:
            self._candidate, self._since = raw, now
        elif now - self._since >= self.debounce_s:
            self.state, self._candidate = raw, None
        return self.state


class EdgeCounter:
    """Counts level changes within a sliding window (speed signal)."""

    def __init__(self, window_s: float) -> None:
        self.window_s = window_s
        self._edges: deque[float] = deque()
        self._last: bool | None = None

    def update(self, raw: bool, now: float) -> int:
        if self._last is not None and raw != self._last:
            self._edges.append(now)
        self._last = raw
        while self._edges and now - self._edges[0] > self.window_s:
            self._edges.popleft()
        return len(self._edges)


class MedianFilter:
    def __init__(self, window_s: float) -> None:
        self.window_s = window_s
        self._samples: deque[tuple[float, float]] = deque()

    def update(self, value: float, now: float) -> float:
        self._samples.append((now, value))
        while self._samples and now - self._samples[0][0] > self.window_s:
            self._samples.popleft()
        return statistics.median(v for _, v in self._samples)

    def reset(self) -> None:
        self._samples.clear()


def sensor_resistance(v_ai: float, v_ref: float, pullup_ohm: float) -> float:
    """Resistance of a sensor against ground, measured through a pull-up from ``v_ref``."""
    if v_ai >= v_ref:
        return math.inf
    if v_ai <= 0:
        return 0.0
    return pullup_ohm * v_ai / (v_ref - v_ai)


def resistance_to_voltage(ohm: float, v_ref: float, pullup_ohm: float) -> float:
    """Inverse of :func:`sensor_resistance` (simulator)."""
    return v_ref * ohm / (pullup_ohm + ohm)


def classify_resistance(ohm: float, open_ohm: float, short_ohm: float) -> Quality:
    if ohm >= open_ohm:
        return Quality.OPEN
    if ohm <= short_ohm:
        return Quality.SHORT
    return Quality.OK


class SteinhartHart:
    """NTC curve 1/T = A + B ln R + C (ln R)^3 from three (ohm, degC) points."""

    def __init__(self, points: list[list[float]]) -> None:
        if len(points) != 3:
            raise ValueError("Steinhart-Hart needs exactly three calibration points")
        (r1, t1), (r2, t2), (r3, t3) = ((float(r), float(t)) for r, t in points)
        l1, l2, l3 = math.log(r1), math.log(r2), math.log(r3)
        y1, y2, y3 = 1 / (t1 + 273.15), 1 / (t2 + 273.15), 1 / (t3 + 273.15)
        g2 = (y2 - y1) / (l2 - l1)
        g3 = (y3 - y1) / (l3 - l1)
        self.c = (g3 - g2) / (l3 - l2) / (l1 + l2 + l3)
        self.b = g2 - self.c * (l1 * l1 + l1 * l2 + l2 * l2)
        self.a = y1 - (self.b + l1 * l1 * self.c) * l1

    def temp_c(self, ohm: float) -> float:
        ln = math.log(ohm)
        return 1 / (self.a + self.b * ln + self.c * ln**3) - 273.15

    def ohm(self, temp_c: float) -> float:
        """Inverse by bisection (simulator only)."""
        lo, hi = 1.0, 10_000_000.0
        for _ in range(100):
            mid = math.sqrt(lo * hi)
            if self.temp_c(mid) > temp_c:  # NTC: higher R -> lower T
                lo = mid
            else:
                hi = mid
        return math.sqrt(lo * hi)


class StepCurve:
    """Discrete tank levels from nominal resistances, with hysteresis at the boundaries."""

    def __init__(self, ohm: list[float], labels: list[str], hysteresis_pct: float) -> None:
        if len(ohm) != len(labels) or len(ohm) < 2:
            raise ValueError("step curve needs one nominal resistance per label")
        self.ohm = [float(o) for o in ohm]
        self.labels = labels
        self.hyst = hysteresis_pct / 100.0

    def _boundary(self, i: int) -> float:
        return (self.ohm[i] + self.ohm[i + 1]) / 2

    def classify(self, ohm: float, current: int | None) -> int:
        nearest = min(range(len(self.ohm)), key=lambda i: abs(self.ohm[i] - ohm))
        if current is None or nearest == current:
            return nearest
        lo, hi = sorted((current, nearest))
        b = self._boundary(lo if nearest > current else hi - 1)
        margin = b * self.hyst
        crossed_far = abs(ohm - b) >= margin
        return nearest if crossed_far else current


def build_curve(spec: Any) -> SteinhartHart | StepCurve | None:
    """Curve from a profile entry, or ``None`` if it is not calibrated yet."""
    if not isinstance(spec, dict):
        return None
    kind = spec.get("type")
    if kind == "steinhart_hart":
        points: Any = spec.get("points")
        return SteinhartHart(points) if is_cal(points) else None
    if kind == "steps":
        ohm: Any = spec.get("ohm")
        if not is_cal(ohm):
            return None
        return StepCurve(ohm, spec["labels"], float(spec.get("hysteresis_pct", 10)))
    raise ValueError(f"unknown curve type {kind!r}")
