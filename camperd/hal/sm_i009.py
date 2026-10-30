"""Sequent Microsystems SM-I-009 Sixteen LV Digital Inputs card (I2C).

Register map and access pattern taken from the vendor code
https://github.com/SequentMicrosystems/16inpind-rpi (src/data.h, src/16in.c,
src/board.c, python/lib16inpind); addresses checked by compiling data.h.

- I2C address (0x20 + stack level) XOR 7, i.e. stack 0 = 0x27
- inputs via the I/O-expander-compatible port register 0x00 (16 bit, active low,
  reversed bit order); works on hardware V2 (expander) and V3 (MCU)
- edge counters and revision registers exist only on the MCU version (V3)
"""

from __future__ import annotations

import threading

from camperd.hal.sm_h001 import Bus, CardError

ADDRESS_BASE = 0x20

REG_INPORT = 0x00  # u16, bit 15 = input 1 ... bit 0 = input 16, low = active
REG_OPTO_IT_RISING = 0x12  # u16, bit 0 = input 1
REG_OPTO_IT_FALLING = 0x14
REG_OPTO_EDGE_COUNT = 0x19  # 16 x u32 little endian
REG_REV_HW_MAJOR = 0xFB  # hw major, hw minor, fw major, fw minor

# vendor pinMask: input n is bit (16 - n) of the port register
PIN_MASK = [1 << (15 - i) for i in range(16)]
CHANNELS = 16
RETRIES = 10


class SmI009:
    def __init__(self, bus: Bus, stack: int, lock: threading.Lock | None = None) -> None:
        if not 0 <= stack <= 7:
            raise ValueError("stack level must be 0..7")
        self.bus = bus
        self.stack = stack
        self.addr = (ADDRESS_BASE + stack) ^ 0x07
        self.lock = lock or threading.Lock()
        self.version: tuple[int, int, int, int] | None = None

    def _io(self, fn, *args):  # type: ignore[no-untyped-def]
        with self.lock:
            try:
                return fn(*args)
            except OSError as exc:
                raise CardError(f"SM-I-009 stack {self.stack} (0x{self.addr:02x}): {exc}") from exc

    def probe(self) -> tuple[int, int, int, int] | None:
        """Checks that the card answers. Returns the revision on V3 (MCU) cards, else None."""
        self._io(self.bus.read_word_data, self.addr, REG_INPORT)
        try:
            b = self._io(self.bus.read_i2c_block_data, self.addr, REG_REV_HW_MAJOR, 4)
        except CardError:
            self.version = None  # V2: plain I/O expander, no revision registers
        else:
            self.version = (b[0], b[1], b[2], b[3])
        return self.version

    @property
    def has_counters(self) -> bool:
        return self.version is not None and self.version[0] >= 3

    def read_inputs(self) -> list[bool]:
        """16 levels, index 0 = input 1; True = current through the opto (voltage present)."""

        def run() -> int:
            prev = -1
            for _ in range(RETRIES):
                val = self.bus.read_word_data(self.addr, REG_INPORT)
                if val == prev:
                    return int(val)
                prev = val
            raise CardError("inputs: no stable read")

        raw: int = self._io(run)
        return [not raw & PIN_MASK[i] for i in range(CHANNELS)]

    def enable_edge_count(self, ch: int, rising: bool = True, falling: bool = True) -> None:
        if not self.has_counters:
            raise CardError("edge counters need SM-I-009 hardware V3")
        bit = 1 << (ch - 1)

        def run() -> None:
            for reg, on in ((REG_OPTO_IT_RISING, rising), (REG_OPTO_IT_FALLING, falling)):
                val = self.bus.read_word_data(self.addr, reg)
                val = (val | bit) if on else (val & ~bit)
                self.bus.write_word_data(self.addr, reg, val & 0xFFFF)

        self._io(run)

    def read_count(self, ch: int) -> int:
        def run() -> int:
            prev = -1
            for _ in range(RETRIES):
                b = self.bus.read_i2c_block_data(self.addr, REG_OPTO_EDGE_COUNT + 4 * (ch - 1), 4)
                val = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24
                if prev >= 0 and (val & 0xFFFFFFFC) == (prev & 0xFFFFFFFC):
                    return val
                prev = val
            raise CardError(f"counter {ch}: no stable read")

        result: int = self._io(run)
        return result
