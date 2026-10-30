"""S10: main-cycle watchdog.

Runs in its own thread: an asyncio task cannot notice a blocked event loop, because
it would be blocked too. If the cycle has not ticked for ``timeout_s`` it switches all
outputs off and terminates the process; systemd restarts it.
"""

from __future__ import annotations

import logging
import os
import threading
import time
from collections.abc import Callable

log = logging.getLogger(__name__)


class Watchdog:
    def __init__(
        self,
        all_off: Callable[[], None],
        timeout_s: float = 2.0,
        on_trip: Callable[[], None] | None = None,
        now: Callable[[], float] = time.monotonic,
    ) -> None:
        self.all_off = all_off
        self.timeout_s = timeout_s
        self.on_trip = on_trip or (lambda: os._exit(70))
        self.now = now
        self._last = now()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.tripped = False

    def kick(self) -> None:
        self._last = self.now()

    def check(self) -> bool:
        """One watchdog check; returns True if it tripped."""
        if self.now() - self._last <= self.timeout_s:
            return False
        self.tripped = True
        log.critical("main cycle stalled for > %.1f s: all outputs off, exiting", self.timeout_s)
        try:
            self.all_off()
        finally:
            self.on_trip()
        return True

    def start(self) -> None:
        self._last = self.now()
        self._thread = threading.Thread(target=self._run, name="watchdog", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()

    def _run(self) -> None:
        while not self._stop.wait(0.2):
            if self.check():
                return
