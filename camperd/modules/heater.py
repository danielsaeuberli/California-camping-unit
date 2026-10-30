"""Parking heater (J604): S+ via K4, setpoint ladder on card B (S7), phases incl. afterrun."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from camperd.clock import Clock
from camperd.profile import Profile, ProfileError, is_cal


@dataclass(frozen=True)
class HeaterConfig:
    levels: int
    ladder_channels: tuple[int, ...]  # relay channels, bit 0 first
    base_ohm: float | None
    branch_ohm: tuple[float, ...] | None
    switch_gap_s: float
    start_timeout_s: float
    afterrun_expected_s: float
    level_to_c: tuple[float, ...] | None

    @property
    def ladder_calibrated(self) -> bool:
        return self.base_ohm is not None and self.branch_ohm is not None

    @classmethod
    def from_profile(cls, p: Profile) -> HeaterConfig:
        lad = "outputs.heater_ladder"
        if p.require(f"{lad}.topology") != "parallel":
            raise ProfileError(f"{lad}.topology: only 'parallel' is supported")
        branches = p.require(f"{lad}.branches")
        channels = []
        for b in branches:
            ch = str(b["channel"])
            if not ch.startswith("K"):
                raise ProfileError(f"{lad}.branches: expected K<n>, got {ch!r}")
            channels.append(int(ch[1:]))
        base = p.get(f"{lad}.base_ohm")
        ohms = [b.get("ohm") for b in branches]
        l2c = p.get("heater.level_to_c")
        levels = int(p.require("heater.levels"))
        if not 2 <= levels <= 2 ** len(channels):
            raise ProfileError(f"heater.levels must be 2..{2 ** len(channels)} for {len(channels)} ladder relays")
        return cls(
            levels=levels,
            ladder_channels=tuple(channels),
            base_ohm=float(base) if is_cal(base) else None,
            branch_ohm=tuple(float(o) for o in ohms) if all(is_cal(o) for o in ohms) else None,
            switch_gap_s=float(p.require(f"{lad}.switch_gap_ms")) / 1000,
            start_timeout_s=float(p.require("heater.start_timeout_s")),
            afterrun_expected_s=float(p.require("heater.afterrun_expected_s")),
            level_to_c=tuple(float(x) for x in l2c) if is_cal(l2c) else None,
        )


class Ladder:
    """Moves the H relays towards the target bit pattern one relay at a time (S7)."""

    def __init__(self, n: int, gap_s: float) -> None:
        self.n = n
        self.gap_s = gap_s
        self.bits = [False] * n
        self._last_switch = -1e9

    def tick(self, target_level: int, now: float) -> list[bool]:
        target = [bool((target_level >> i) & 1) for i in range(self.n)]
        if target != self.bits and now - self._last_switch >= self.gap_s:
            i = next(i for i in range(self.n) if target[i] != self.bits[i])
            self.bits[i] = target[i]
            self._last_switch = now
        return list(self.bits)


class Heater:
    def __init__(self, cfg: HeaterConfig, clock: Clock) -> None:
        self.cfg = cfg
        self.clock = clock
        self.on = False
        self.level = cfg.levels // 2 if cfg.ladder_calibrated else 0
        self.phase = "off"
        self._phase_since = clock.now()
        self.ladder = Ladder(len(cfg.ladder_channels), cfg.switch_gap_s)
        self.running = False

    def set(self, on: bool | None = None, level: int | None = None) -> str | None:
        if level is not None:
            if not self.cfg.ladder_calibrated:
                return "ladder_uncalibrated"
            lv = int(level)
            if not 0 <= lv < self.cfg.levels:
                return "out_of_range"
            self.level = lv
        if on is not None and bool(on) != self.on:
            self.on = bool(on)
            now = self.clock.now()
            if self.on:
                self._enter("start", now)
            elif self.phase in ("start", "heating") or self.phase == "fault":
                self._enter("afterrun" if self.running else "off", now)
        return None

    def _enter(self, phase: str, now: float) -> None:
        self.phase = phase
        self._phase_since = now

    def tick(self, running: bool | None) -> tuple[bool, list[bool]]:
        """``running`` = DI4 operating indication. Returns (K4, [H1..H4])."""
        now = self.clock.now()
        self.running = bool(running)
        if self.phase == "start":
            if self.running:
                self._enter("heating", now)
            elif now - self._phase_since >= self.cfg.start_timeout_s:
                self._enter("fault", now)
        elif self.phase == "afterrun" and not self.running:
            self._enter("off", now)
        elif self.phase == "off" and self.running:
            self._enter("afterrun", now)  # e.g. after restart while the heater still runs on
        k4 = self.on and self.phase in ("start", "heating")
        # Ladder relays stay unpowered (base resistance = level 0) while the heater is off
        # and as long as the ladder resistors are not in the profile.
        target = self.level if self.on and self.cfg.ladder_calibrated else 0
        return k4, self.ladder.tick(target, now)

    def ladder_ohm(self) -> float | None:
        """Resistance seen by J604 at T2p (parallel ladder), ``None`` if not calibrated."""
        if self.cfg.base_ohm is None or self.cfg.branch_ohm is None:
            return None
        g = 1 / self.cfg.base_ohm + sum(1 / o for o, b in zip(self.cfg.branch_ohm, self.ladder.bits, strict=True) if b)
        return 1 / g

    def afterrun_left_s(self) -> int:
        if self.phase != "afterrun":
            return 0
        return max(0, int(self.cfg.afterrun_expected_s - (self.clock.now() - self._phase_since)))

    def state(self) -> dict[str, Any]:
        return {
            "on": self.on,
            "level": self.level,
            "phase": self.phase,
            "afterrun_s": self.afterrun_left_s(),
            "level_c": self.cfg.level_to_c[self.level] if self.cfg.level_to_c else None,
            "level_ok": self.cfg.ladder_calibrated,
            "levels": self.cfg.levels,
        }

    def alerts(self) -> list[dict[str, str]]:
        return [{"id": "heater_fault", "level": "err"}] if self.phase == "fault" else []
