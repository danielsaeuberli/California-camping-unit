"""Roof (J768) logic. Pure logic, no I/O; time from the injected clock.

Two ways to run, both driven by run messages from the UI every 100 ms:
- hold-to-run: messages while the button is held;
- tap-to-run (``auto``): one tap, the UI keeps sending until STOP, any touch, or the
  end position. Only available once the end positions (truth table) and the relay
  direction are measured; camperd stops at the end position itself.

Safety rules: S2 (never K1+K2, 300 ms gap on reversal), S3 (run messages stop / socket
loss / max run time -> stop), S4 (no opening while driving, ignition required), S5
(stop when the DI card fails).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from camperd.clock import Clock
from camperd.profile import Profile, is_cal

DIRS = ("open", "close")
TARGET = {"open": "open", "close": "closed"}


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
    tap_to_run: bool = False
    pump_afterrun_s: float | None = None  # J768 pump run-on after the command ends

    @property
    def settle_s(self) -> float:
        """Time after a stop before the roof counts as idle and may reverse."""
        # OPEN: pump run-on not measured yet -> only the reversal gap applies
        return max(self.reverse_gap_s, self.pump_afterrun_s or 0.0)

    @property
    def tap_available(self) -> bool:
        """One-tap runs need known end positions and a verified relay direction."""
        return self.tap_to_run and self.truth_table is not None and self.direction_verified

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
            tap_to_run=bool(p.get("roof.tap_to_run", False)),
            pump_afterrun_s=float(ar) if is_cal(ar := p.get("roof.pump_afterrun_s")) else None,
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
        self._latched_end = False
        self.auto = False
        self._inputs = RoofInputs(None, None, False, True)
        self.k1 = False
        self.k2 = False

    # ----- commands -----
    def hold(self, direction: str, seq: int, auto: bool = False) -> str | None:
        """Run message from the UI (every 100 ms). Returns a reject reason or None."""
        if direction not in DIRS:
            return "bad_dir"
        if auto and not self.cfg.tap_available:
            return "tap_unavailable"
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
            if auto and self.end_position() == TARGET[direction]:
                return "at_end"  # already there: nothing to do
            self.cmd = direction
            self._press_start = now
            self._latched_timeout = False
            self._latched_end = False
            self.auto = auto
            self.block = "none"
        elif seq <= self._last_seq:
            return "stale_seq"
        self._last_seq = seq
        self._last_hold = now
        if self._latched_timeout:
            return "timeout"
        if self._latched_end:
            return "at_end"
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
            elif self.auto and self.end_position() == TARGET[self.cmd]:
                self._deenergize(now)  # tap-to-run: stop at the end position
                self._latched_end = True
        if self.cmd is not None and not self._latched_timeout and not self._latched_end and self._energized != self.cmd:
            reversing = self._last_energized_dir not in (None, self.cmd)
            gap_ok = (now - self._off_since >= self.cfg.settle_s) if reversing else True
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

    @property
    def afterrun(self) -> bool:
        """Command off, but the pump may still run on (pump_afterrun_s)."""
        return (
            not self.moving
            and self._last_energized_dir is not None
            and self.clock.now() - self._off_since < self.cfg.settle_s
        )

    @property
    def busy(self) -> bool:
        """Moving or pump run-on: no shutdown, no idle return (S8)."""
        return self.moving or self.afterrun

    def position(self) -> str:
        if self.moving:
            return "moving"
        return self.end_position()

    def end_position(self) -> str:
        """Position from the J768 feedback only (also while the pump runs)."""
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
            "tap": self.cfg.tap_available,
            "auto": self.auto and self.cmd is not None,
            "afterrun": self.afterrun,
        }

    def alerts(self) -> list[dict[str, str]]:
        pos = self.position()
        out = []
        if self._inputs.ignition and pos not in ("closed", "unknown"):
            out.append({"id": "roof_open_ignition", "level": "err" if self._inputs.vehicle_moving else "warn"})
        return out
