"""Common driver interface (SPEC 3.2).

Channels are 1-based like the documentation: DI1-DI16, AI1-AI8, relays K1-K8,
ladder GPIOs H1-H4.
"""

from __future__ import annotations

import asyncio
from abc import ABC, abstractmethod
from typing import Any

N_DI = 16
N_AI = 8
N_RELAY = 8
N_GPIO = 4


class HalError(IOError):
    """A bus transaction failed (counts towards S5)."""


class Hal(ABC):
    name = "base"

    def __init__(self) -> None:
        self.events: asyncio.Queue[dict[str, Any]] = asyncio.Queue()

    async def start(self) -> None:  # noqa: B027 - optional hook
        pass

    async def stop(self) -> None:  # noqa: B027 - optional hook
        pass

    @abstractmethod
    async def read_di(self) -> list[bool]:
        """16 levels, True = voltage present. Raises HalError on bus failure."""

    @abstractmethod
    async def read_ai(self) -> list[float | None]:
        """8 voltages at the AI terminals. Raises HalError on bus failure."""

    @abstractmethod
    async def write_relay(self, ch: int, on: bool) -> None: ...

    @abstractmethod
    async def write_gpio(self, ch: int, on: bool) -> None: ...

    @abstractmethod
    def all_off_sync(self) -> None:
        """Switch every output off, synchronously and thread-safe (watchdog, S10)."""

    async def all_off(self) -> None:
        for ch in range(1, N_RELAY + 1):
            await self.write_relay(ch, False)
        for ch in range(1, N_GPIO + 1):
            await self.write_gpio(ch, False)
