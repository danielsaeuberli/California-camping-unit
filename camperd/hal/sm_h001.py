"""Sequent Microsystems SM-H-001 Home Automation card (I2C).

Register map and access pattern taken from the vendor code
https://github.com/SequentMicrosystems/ioplus-rpi (src/ioplus.h, src/opto.c,
python/libioplus/__init__.py); addresses checked by compiling ioplus.h.

- I2C address 0x28 + stack level (0-7, DIP switches ID0-ID2)
- 8 relays (N.O.), 8 opto inputs with edge counters (<= 500 Hz), 8 ADC 0-3.3 V
- reads repeat until two values agree ("anti-spurious", as the vendor does)
"""

from __future__ import annotations

import threading
from typing import Protocol

ADDRESS_BASE = 0x28

REG_RELAY_VAL = 0x00
REG_RELAY_SET = 0x01
REG_RELAY_CLR = 0x02
REG_OPTO_IN = 0x03
REG_ADC_MV = 0x18  # 8 x u16 little endian, millivolts
REG_OPTO_IT_RISING = 0x38
REG_OPTO_IT_FALLING = 0x39
REG_OPTO_CNT_RST = 0x3C
REG_DIAG_TEMP = 0x3E  # u8 CPU temperature, then u16 LE 3.3 V rail in mV (0x3F)
REG_RELAY_DEFAULT = 0x62  # relay state the card restores at power-up
REG_REV_HW_MAJOR = 0x78  # hw major, hw minor, fw major, fw minor
REG_OPTO_EDGE_COUNT = 0x80  # 8 x u32 little endian

RETRIES = 10
CHANNELS = 8


class Bus(Protocol):
    """Subset of smbus2.SMBus used here (lets tests inject a fake)."""

    def read_byte_data(self, i2c_addr: int, register: int) -> int: ...
    def write_byte_data(self, i2c_addr: int, register: int, value: int) -> None: ...
    def read_word_data(self, i2c_addr: int, register: int) -> int: ...
    def read_i2c_block_data(self, i2c_addr: int, register: int, length: int) -> list[int]: ...


class CardError(IOError):
    pass


class SmH001:
    def __init__(self, bus: Bus, stack: int, lock: threading.Lock | None = None) -> None:
        if not 0 <= stack <= 7:
            raise ValueError("stack level must be 0..7")
        self.bus = bus
        self.stack = stack
        self.addr = ADDRESS_BASE + stack
        self.lock = lock or threading.Lock()
        self.version: tuple[int, int, int, int] | None = None

    # ----- low level -----
    def _read_byte_as(self, reg: int) -> int:
        prev = -1
        for _ in range(RETRIES):
            val = self.bus.read_byte_data(self.addr, reg)
            if val == prev:
                return val
            prev = val
        raise CardError(f"0x{self.addr:02x} reg 0x{reg:02x}: no stable read")

    def _read_word_as(self, reg: int, mask: int = 0xFFFC) -> int:
        prev = -1
        for _ in range(RETRIES):
            val = self.bus.read_word_data(self.addr, reg)
            if prev >= 0 and (val & mask) == (prev & mask):
                return val
            prev = val
        raise CardError(f"0x{self.addr:02x} reg 0x{reg:02x}: no stable read")

    def _io(self, fn, *args):  # type: ignore[no-untyped-def]
        with self.lock:
            try:
                return fn(*args)
            except OSError as exc:
                raise CardError(f"SM-H-001 stack {self.stack} (0x{self.addr:02x}): {exc}") from exc

    # ----- identification -----
    def probe(self) -> tuple[int, int, int, int]:
        """Reads hardware/firmware revision; raises CardError if the card does not answer."""

        def run() -> tuple[int, int, int, int]:
            b = self.bus.read_i2c_block_data(self.addr, REG_REV_HW_MAJOR, 4)
            return b[0], b[1], b[2], b[3]

        self.version = self._io(run)
        return self.version

    # ----- relays -----
    def read_relays(self) -> int:
        return int(self._io(self._read_byte_as, REG_RELAY_VAL))

    def set_relay(self, ch: int, on: bool) -> None:
        """Switch one relay via the SET/CLR registers (single channel, S7), then verify."""
        if not 1 <= ch <= CHANNELS:
            raise ValueError("relay channel 1..8")

        def run() -> None:
            self.bus.write_byte_data(self.addr, REG_RELAY_SET if on else REG_RELAY_CLR, ch)
            val = self._read_byte_as(REG_RELAY_VAL)
            if bool(val & (1 << (ch - 1))) != on:
                raise CardError(f"relay {ch} on stack {self.stack} did not switch {'on' if on else 'off'}")

        self._io(run)

    def write_relays(self, mask: int) -> None:
        self._io(self.bus.write_byte_data, self.addr, REG_RELAY_VAL, mask & 0xFF)

    def relay_default(self) -> int:
        return int(self._io(self._read_byte_as, REG_RELAY_DEFAULT))

    def set_relay_default(self, mask: int) -> None:
        self._io(self.bus.write_byte_data, self.addr, REG_RELAY_DEFAULT, mask & 0xFF)

    # ----- inputs -----
    def read_opto(self) -> int:
        """Bit n-1 set = current through opto input n (voltage present)."""
        return int(self._io(self._read_byte_as, REG_OPTO_IN))

    def read_adc_v(self) -> list[float]:
        def run() -> list[float]:
            return [self._read_word_as(REG_ADC_MV + 2 * i) / 1000.0 for i in range(CHANNELS)]

        result: list[float] = self._io(run)
        return result

    def read_rail_v(self) -> float:
        """The card's own 3.3 V rail (pull-up and ADC supply), in volts."""

        def run() -> float:
            prev = -1
            for _ in range(RETRIES):
                b = self.bus.read_i2c_block_data(self.addr, REG_DIAG_TEMP, 3)
                mv = b[1] | b[2] << 8
                if prev >= 0 and (mv & 0xFFFC) == (prev & 0xFFFC):
                    return mv / 1000.0
                prev = mv
            raise CardError("rail voltage: no stable read")

        result: float = self._io(run)
        return result

    def enable_edge_count(self, ch: int, rising: bool = True, falling: bool = True) -> None:
        bit = 1 << (ch - 1)

        def run() -> None:
            r = self.bus.read_byte_data(self.addr, REG_OPTO_IT_RISING)
            f = self.bus.read_byte_data(self.addr, REG_OPTO_IT_FALLING)
            r = (r | bit) if rising else (r & ~bit)
            f = (f | bit) if falling else (f & ~bit)
            self.bus.write_byte_data(self.addr, REG_OPTO_IT_RISING, r & 0xFF)
            self.bus.write_byte_data(self.addr, REG_OPTO_IT_FALLING, f & 0xFF)

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
