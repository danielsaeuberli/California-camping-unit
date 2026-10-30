"""Injectable clocks so state machines can be tested without waiting."""

from __future__ import annotations

import time
from typing import Protocol


class Clock(Protocol):
    def now(self) -> float:
        """Monotonic seconds."""
        ...

    def wall_ms(self) -> int:
        """Wall-clock milliseconds since the epoch (for timestamps sent to the UI)."""
        ...


class SystemClock:
    def now(self) -> float:
        return time.monotonic()

    def wall_ms(self) -> int:
        return int(time.time() * 1000)


class FakeClock:
    """Manually advanced clock for tests and the simulator's unit tests."""

    def __init__(self, start: float = 1000.0) -> None:
        self._t = start

    def now(self) -> float:
        return self._t

    def wall_ms(self) -> int:
        return int(self._t * 1000)

    def advance(self, seconds: float) -> None:
        self._t += seconds
