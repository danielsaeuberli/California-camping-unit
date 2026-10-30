"""Roof (J768) hold-to-run logic. Pure logic, no I/O; time from the injected clock.

Safety rules: S2 (never K1+K2, 300 ms gap on reversal), S3 (hold timeout, socket
loss, max run time), S4 (no opening while driving), S5 (stop when DI card fails).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from camperd.clock import Clock
from camperd.profile import Profile, is_cal

DIRS = ("open", "close")


@dataclass(frozen=True)
class RoofConfig:
    hold_timeout_s: float
    reverse_gap_s: float
    max_run_s: float
    block_open_when_moving: bool
    require_ignition: bool
    truth_table: dict[str, str] | None
    k1_dir: str
    direction_verified: bool

    @classmethod
    def from_profile(cls, p: Profile) -> RoofConfig:
        table = p.get("roof.truth_table")
        meaning = p.get("outputs.roof_a.meaning")
        verified = is_cal(meaning) and meaning in DIRS
        # OPEN: O6 - until roof_a.meaning is measured we assume K1 = open (as in the
        # UI prototype) and treat the direction as unverified (see hold()).
        k1_dir = meaning if verified else "open"
        return cls(
            hold_timeout_s=float(p.require("roof.hold_timeout_ms")) / 1000,
            reverse_gap_s=float(p.require("roof.reverse_gap_ms")) / 1000,
            max_run_s=float(p.require("roof.max_run_s")),
            block_open_when_moving=bool(p.require("roof.block_open_when_moving")),
            require_ignition=bool(p.get("roof.require_ignition", True)),
            truth_table={str(k): str(v) for k, v in table.items()} if isinstance(table, dict) else None,
            k1_dir=k1_dir,
            direction_verified=verified,
        )


@dataclass(frozen=True)
class RoofInputs:
    fb_a: bool | None
    fb_b: bool | None
    vehicle_moving: bool
    di_ok: bool
    ignition: bool = False


class Roof:
    def __init__(self, cfg: RoofConfig, clock: Clock) -> None:
        self.cfg = cfg
        self.clock = clock
        self.cmd: str | None = None  # direction requested by the current press
        self.block = "none"
        self._press_start = 0.0
        self._last_hold = -1e9
        self._last_seq = -1
        self._energized: str | None = None  # direction whose relay is on
        self._off_since = -1e9
        self._last_energized_dir: str | None = None
        self._latched_timeout = False
        self._inputs = RoofInputs(None, None, False, True)
        self.k1 = False
        self.k2 = False

    # ----- commands -----
    def hold(self, direction: str, seq: int) -> str | None:
        """Hold message from the UI (every 100 ms). Returns a reject reason or None."""
        if direction not in DIRS:
            return "bad_dir"
        now = self.clock.now()
        if not self._inputs.di_ok:
            self.block = "unit_io"
            return "blocked_io"
        if self.cfg.require_ignition and not self._inputs.ignition:
            self.block = "ignition"
            return "blocked_ignition"
        if self._inputs.vehicle_moving:
            if direction == "open" and self.cfg.block_open_when_moving:
                self.block = "moving"
                return "blocked_moving"
            if not self.cfg.direction_verified:
                # Unmeasured relay meaning: "close" could actually open. Block both.
                self.block = "moving"
                return "blocked_moving"
        new_press = now - self._last_hold > self.cfg.hold_timeout_s or direction != self.cmd
        if new_press:
            if self.cmd is not None and direction != self.cmd:
                self._deenergize(now)
            self.cmd = direction
            self._press_start = now
            self._latched_timeout = False
            self.block = "none"
        elif seq <= self._last_seq:
            return "stale_seq"
        self._last_seq = seq
        self._last_hold = now
        if self._latched_timeout:
            return "timeout"
        return None

    def release(self) -> None:
        """Explicit release or WebSocket loss: stop immediately."""
        self._deenergize(self.clock.now())
        self.cmd = None
        self._last_hold = -1e9

    # ----- cycle -----
    def tick(self, inputs: RoofInputs) -> tuple[bool, bool]:
        self._inputs = inputs
        now = self.clock.now()
        if self.cmd is not None:
            if now - self._last_hold > self.cfg.hold_timeout_s:
                self.release()  # S3: hold messages stopped
            elif self.cfg.require_ignition and not inputs.ignition:
                self._deenergize(now)  # ignition switched off while moving the roof
                self.cmd = None
                self.block = "ignition"
            elif not inputs.di_ok:
                self._deenergize(now)  # S5
                self.cmd = None
                self.block = "unit_io"
            elif inputs.vehicle_moving and self.cmd == "open" and self.cfg.block_open_when_moving:
                self._deenergize(now)  # S4: stop opening when driving starts
                self.cmd = None
                self.block = "moving"
            elif inputs.vehicle_moving and not self.cfg.direction_verified:
                self._deenergize(now)
                self.cmd = None
                self.block = "moving"
            elif now - self._press_start >= self.cfg.max_run_s:
                self._deenergize(now)  # S3: max run time per press
                self._latched_timeout = True
                self.block = "timeout"
        if self.cmd is not None and not self._latched_timeout and self._energized != self.cmd:
            reversing = self._last_energized_dir not in (None, self.cmd)
            gap_ok = (now - self._off_since >= self.cfg.reverse_gap_s) if reversing else True
            if self._energized is None and gap_ok:
                self._energized = self.cmd
                self._last_energized_dir = self.cmd
        if self.cmd is None and not inputs.vehicle_moving and inputs.di_ok and self.block == "moving":
            self.block = "none"
        if self.cmd is None and inputs.di_ok and self.block == "unit_io":
            self.block = "none"
        if self.cmd is None and inputs.ignition and self.block == "ignition":
            self.block = "none"
        self.k1 = self._energized == self.cfg.k1_dir
        self.k2 = self._energized is not None and not self.k1
        assert not (self.k1 and self.k2)  # S2
        return self.k1, self.k2

    def _deenergize(self, now: float) -> None:
        if self._energized is not None:
            self._off_since = now
        self._energized = None
        self.k1 = self.k2 = False

    # ----- state -----
    @property
    def moving(self) -> bool:
        return self._energized is not None

    def position(self) -> str:
        if self.moving:
            return "moving"
        a, b = self._inputs.fb_a, self._inputs.fb_b
        if not self._inputs.di_ok or a is None or b is None:
            return "unknown"
        if self.cfg.truth_table is None:
            return "unknown"
        return self.cfg.truth_table.get(f"{int(a)}{int(b)}", "unknown")

    def state(self) -> dict[str, Any]:
        return {
            "pos": self.position(),
            "cmd": self.cmd or "none",
            "block": self.block,
            "direction_verified": self.cfg.direction_verified,
            "require_ignition": self.cfg.require_ignition,
        }

    def alerts(self) -> list[dict[str, str]]:
        pos = self.position()
        out = []
        if self._inputs.ignition and pos not in ("closed", "unknown"):
            out.append({"id": "roof_open_ignition", "level": "err" if self._inputs.vehicle_moving else "warn"})
        return out
