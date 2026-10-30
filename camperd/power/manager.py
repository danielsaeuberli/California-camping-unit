"""Operating states drive / afterrun / standby / shutdown (SPEC 10.1, rule S8)."""

from __future__ import annotations

from dataclasses import dataclass

from camperd.clock import Clock
from camperd.profile import Profile


@dataclass(frozen=True)
class PowerConfig:
    afterrun_to_standby_s: float
    standby_to_shutdown_s: float
    undervoltage_v: float
    undervoltage_hold_s: float

    @classmethod
    def from_profile(cls, p: Profile) -> PowerConfig:
        return cls(
            afterrun_to_standby_s=float(p.require("power.afterrun_to_standby_s")),
            standby_to_shutdown_s=float(p.require("power.standby_to_shutdown_s")),
            undervoltage_v=float(p.require("power.undervoltage_v")),
            undervoltage_hold_s=float(p.require("power.undervoltage_hold_s")),
        )


@dataclass(frozen=True)
class PowerInputs:
    ignition: bool
    fridge_active: bool
    heater_phase: str
    roof_moving: bool
    battery_v: float | None


class PowerManager:
    def __init__(self, cfg: PowerConfig, clock: Clock) -> None:
        self.cfg = cfg
        self.clock = clock
        self.mode = "afterrun"
        self._last_input = clock.now()
        self._under_since: float | None = None
        self.undervoltage = False
        self.wake_touch_pending = False
        self._standby_since = clock.now()

    def user_input(self) -> None:
        self._last_input = self.clock.now()
        if self.mode == "standby":
            self.mode = "afterrun"
            self.wake_touch_pending = True

    def shutdown_blocked(self, i: PowerInputs) -> bool:
        """S8: never while the heater runs on or the roof moves."""
        return i.heater_phase == "afterrun" or i.roof_moving

    def tick(self, i: PowerInputs) -> str:
        now = self.clock.now()
        idle = now - self._last_input
        self._track_undervoltage(i.battery_v, now)
        if self.mode == "shutdown":
            return self.mode
        if i.ignition:
            self.mode = "drive"
            self._last_input = now
            return self.mode
        if self.mode == "drive":
            self.mode = "afterrun"
            self._last_input = now
        elif self.mode == "afterrun" and idle >= self.cfg.afterrun_to_standby_s:
            self.mode = "standby"
            self._standby_since = now
        if self.undervoltage and not self.shutdown_blocked(i):
            self.mode = "shutdown"
        elif self.mode == "standby" and now - self._standby_since >= self.cfg.standby_to_shutdown_s:
            idle_ok = not i.fridge_active and i.heater_phase == "off" and not i.roof_moving
            if idle_ok and not self.shutdown_blocked(i):
                self.mode = "shutdown"
        return self.mode

    def _track_undervoltage(self, v: float | None, now: float) -> None:
        # Without a Cerbo voltage there is no automatic undervoltage shutdown (SPEC 10.1).
        if v is None or v >= self.cfg.undervoltage_v:
            self._under_since = None
            self.undervoltage = False
            return
        if self._under_since is None:
            self._under_since = now
        self.undervoltage = now - self._under_since >= self.cfg.undervoltage_hold_s

    @property
    def display_on(self) -> bool:
        return self.mode in ("drive", "afterrun")
