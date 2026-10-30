"""Control of the MultiPlus through the Cerbo GX (SPEC 5.1).

Only two VE.Bus settings may be written, and only if ``victron.control`` is true:
``Mode`` (1 charger only, 3 on, 4 off; 2 inverter only if listed in ``victron.modes``)
and the shore current limit ``Ac/In/1/CurrentLimit``. Every write is triggered by a
user action, rate-limited, and confirmed by reading the value back.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from camperd.clock import Clock
from camperd.modules.energy import VictronStore
from camperd.profile import Profile, is_cal

WRITABLE = ("Mode", "Ac/In/1/CurrentLimit")
MODE_NAMES = {1: "charger_only", 2: "inverter_only", 3: "on", 4: "off"}


@dataclass(frozen=True)
class VictronControlConfig:
    enabled: bool
    modes: tuple[int, ...]
    limit_min_a: float | None
    limit_max_a: float | None
    confirm_s: float
    min_interval_s: float

    @classmethod
    def from_profile(cls, p: Profile) -> VictronControlConfig:
        lo = p.get("victron.current_limit_a.min")
        hi = p.get("victron.current_limit_a.max")
        modes = tuple(int(m) for m in p.get("victron.modes", [1, 3, 4]))
        if not set(modes) <= set(MODE_NAMES):
            raise ValueError(f"victron.modes: allowed values are {sorted(MODE_NAMES)}")
        return cls(
            enabled=p.get("victron.control", False) is True,
            modes=modes,
            limit_min_a=float(lo) if is_cal(lo) else None,
            limit_max_a=float(hi) if is_cal(hi) else None,
            confirm_s=float(p.get("victron.confirm_s", 5)),
            min_interval_s=float(p.get("victron.min_interval_s", 2)),
        )

    @property
    def limit_enabled(self) -> bool:
        return self.enabled and self.limit_min_a is not None and self.limit_max_a is not None


class VictronControl:
    def __init__(self, cfg: VictronControlConfig, store: VictronStore, clock: Clock) -> None:
        self.cfg = cfg
        self.store = store
        self.clock = clock
        self._last_write = -1e9
        self.pending: dict[str, Any] = {}  # key -> (expected value, sent at)
        self.unconfirmed: str | None = None

    def request(self, values: dict[str, Any]) -> str | None:
        """UI command ``{"mode": 3}`` or ``{"current_limit_a": 10.0}``. Returns a reject reason."""
        if not self.cfg.enabled:
            return "control_disabled"
        if not self.store.connected or self.store.portal_id is None or self.store.vebus_instance is None:
            return "victron_offline"
        now = self.clock.now()
        if now - self._last_write < self.cfg.min_interval_s:
            return "too_fast"
        writes: list[tuple[str, str, Any]] = []
        if "mode" in values:
            mode = int(values["mode"])
            if mode not in self.cfg.modes:
                return "bad_mode"
            writes.append(("mode", "Mode", mode))
        if "current_limit_a" in values:
            if not self.cfg.limit_enabled:
                return "limit_disabled"
            if self.store.get("vebus.current_limit_adjustable") not in (1, True):
                return "limit_not_adjustable"  # e.g. fixed by a VE.Bus BMS or the panel
            limit = round(float(values["current_limit_a"]), 1)
            assert self.cfg.limit_min_a is not None and self.cfg.limit_max_a is not None
            if not self.cfg.limit_min_a <= limit <= self.cfg.limit_max_a:
                return "out_of_range"
            writes.append(("current_limit", "Ac/In/1/CurrentLimit", limit))
        if not writes:
            return "bad_message"
        for key, path, value in writes:
            assert path in WRITABLE
            self.store.pending_writes.append((path, value))
            self.pending[key] = (value, now)
        self._last_write = now
        self.unconfirmed = None
        return None

    def tick(self) -> None:
        now = self.clock.now()
        for key, (expected, sent) in list(self.pending.items()):
            actual = self.store.get(f"vebus.{key}")
            if actual is not None and abs(float(actual) - float(expected)) < 0.05:
                del self.pending[key]
            elif now - sent > self.cfg.confirm_s:
                del self.pending[key]
                self.unconfirmed = key  # MultiPlus did not take it (front switch, BMS, ...)

    def state(self) -> dict[str, Any]:
        return {
            "enabled": self.cfg.enabled,
            "modes": list(self.cfg.modes),
            "limit_min_a": self.cfg.limit_min_a if self.cfg.limit_enabled else None,
            "limit_max_a": self.cfg.limit_max_a if self.cfg.limit_enabled else None,
            "pending": sorted(self.pending),
            "unconfirmed": self.unconfirmed,
        }

    def alerts(self) -> list[dict[str, str]]:
        return [{"id": "victron_unconfirmed", "level": "warn"}] if self.unconfirmed else []
