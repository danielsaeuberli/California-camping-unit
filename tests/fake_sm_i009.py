"""Register-level fake of the SM-I-009 (map from 16inpind-rpi)."""

from __future__ import annotations

from camperd.hal import sm_i009 as r


class FakeInputCard:
    def __init__(self, v3: bool = True) -> None:
        self.inputs = [False] * 16  # True = voltage present
        self.counts = [0] * 16
        self.rising = 0
        self.falling = 0
        self.v3 = v3
        self.rev = [3, 0, 1, 2]

    def port(self) -> int:
        val = 0xFFFF  # active low
        for i, on in enumerate(self.inputs):
            if on:
                val &= ~r.PIN_MASK[i]
        return val


class FakeGpio:
    def __init__(self) -> None:
        self.levels: dict[int, bool] = {}

    def set(self, gpio: int, level: bool) -> None:
        self.levels[gpio] = level
