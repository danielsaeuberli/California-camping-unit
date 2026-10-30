"""Fridge thermostat on G817 with compressor protection (S6) and diagnosis codes."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from camperd.clock import Clock
from camperd.profile import Profile
from camperd.signals import Quality, Value


@dataclass(frozen=True)
class FridgeConfig:
    target_min: int
    target_max: int
    target_default: int
    on_above_k: float
    off_below_k: float
    min_run_s: float
    min_pause_s: float

    @classmethod
    def from_profile(cls, p: Profile) -> FridgeConfig:
        duty = p.get("fridge.fallback_duty", False)
        if duty not in (False, None, "off"):
            # OPEN: fallback_duty other than "off" is not implemented in v1.
            raise NotImplementedError("fridge.fallback_duty other than off")
        return cls(
            target_min=int(p.require("fridge.target_c.min")),
            target_max=int(p.require("fridge.target_c.max")),
            target_default=int(p.require("fridge.target_c.default")),
            on_above_k=float(p.require("fridge.on_above_target_k")),
            off_below_k=float(p.require("fridge.off_below_target_k")),
            min_run_s=float(p.require("fridge.min_run_s")),
            min_pause_s=float(p.require("fridge.min_pause_s")),
        )


class Fridge:
    def __init__(self, cfg: FridgeConfig, clock: Clock) -> None:
        self.cfg = cfg
        self.clock = clock
        self.on = False
        self.target_c = cfg.target_default
        self.request = False
        self._changed = -1e9
        self.temp = Value(None, 0.0, Quality.UNCALIBRATED)
        self.fault: int | None = None

    def set(self, on: bool | None = None, target_c: float | None = None) -> str | None:
        if target_c is not None:
            t = round(float(target_c))
            if not self.cfg.target_min <= t <= self.cfg.target_max:
                return "out_of_range"
            self.target_c = t
        if on is not None:
            self.on = bool(on)
        return None

    def tick(self, temp: Value, fault_code: int | None) -> bool:
        self.temp = temp
        self.fault = fault_code
        now = self.clock.now()
        want = self.request
        if not self.on or not temp.ok:
            want = False
        elif temp.value >= self.target_c + self.cfg.on_above_k:
            want = True
        elif temp.value <= self.target_c - self.cfg.off_below_k:
            want = False
        if want != self.request:
            held = now - self._changed
            limit = self.cfg.min_run_s if self.request else self.cfg.min_pause_s
            if held >= limit:  # S6
                self.request = want
                self._changed = now
        return self.request

    def thermostat_active(self) -> bool:
        return self.on or self.request

    def state(self) -> dict[str, Any]:
        return {
            "on": self.on,
            "target_c": self.target_c,
            "temp_c": round(self.temp.value, 1) if self.temp.ok else None,
            "temp_q": self.temp.q.value,
            "request": self.request,
            "fault": self.fault,
        }

    def alerts(self) -> list[dict[str, str]]:
        out = []
        if self.fault is not None:
            out.append({"id": f"fridge_fault_{self.fault}", "level": "err"})
        if self.on and not self.temp.ok:
            out.append({"id": "fridge_sensor", "level": "warn"})
        return out
