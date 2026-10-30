"""Register-level fake of the SM-H-001 behind smbus2 (map from ioplus-rpi)."""

from __future__ import annotations

from camperd.hal import sm_h001 as r


class FakeCard:
    def __init__(self) -> None:
        self.relays = 0
        self.relay_default = 0
        self.opto = 0
        self.adc_mv = [0] * 8
        self.counts = [0] * 8
        self.rising = 0
        self.falling = 0
        self.rail_mv = 3300
        self.rev = [4, 0, 1, 7]
        self.stuck_relays = 0  # bits that ignore writes (welded/defective relay)


class FakeBus:
    def __init__(self, cards: dict[int, FakeCard]) -> None:
        self.cards = cards
        self.fail = False
        self.writes: list[tuple[int, int, int]] = []

    def _card(self, addr: int) -> FakeCard:
        if self.fail or addr not in self.cards:
            raise OSError(121, "Remote I/O error")
        return self.cards[addr]

    def read_byte_data(self, addr: int, reg: int) -> int:
        c = self._card(addr)
        return {
            r.REG_RELAY_VAL: c.relays,
            r.REG_OPTO_IN: c.opto,
            r.REG_RELAY_DEFAULT: c.relay_default,
            r.REG_OPTO_IT_RISING: c.rising,
            r.REG_OPTO_IT_FALLING: c.falling,
        }[reg]

    def write_byte_data(self, addr: int, reg: int, value: int) -> None:
        c = self._card(addr)
        self.writes.append((addr, reg, value))
        keep = c.relays & c.stuck_relays
        if reg == r.REG_RELAY_VAL:
            c.relays = (value & ~c.stuck_relays) | keep
        elif reg == r.REG_RELAY_SET:
            c.relays = ((c.relays | 1 << (value - 1)) & ~c.stuck_relays) | keep
        elif reg == r.REG_RELAY_CLR:
            c.relays = ((c.relays & ~(1 << (value - 1))) & ~c.stuck_relays) | keep
        elif reg == r.REG_RELAY_DEFAULT:
            c.relay_default = value
        elif reg == r.REG_OPTO_IT_RISING:
            c.rising = value
        elif reg == r.REG_OPTO_IT_FALLING:
            c.falling = value
        else:
            raise AssertionError(f"unexpected write reg 0x{reg:02x}")

    def read_word_data(self, addr: int, reg: int) -> int:
        c = self._card(addr)
        i = (reg - r.REG_ADC_MV) // 2
        assert 0 <= i < 8 and (reg - r.REG_ADC_MV) % 2 == 0
        return c.adc_mv[i]

    def read_i2c_block_data(self, addr: int, reg: int, length: int) -> list[int]:
        c = self._card(addr)
        if reg == r.REG_REV_HW_MAJOR:
            return c.rev[:length]
        if reg == r.REG_DIAG_TEMP:
            return [35, c.rail_mv & 0xFF, c.rail_mv >> 8][:length]
        i = (reg - r.REG_OPTO_EDGE_COUNT) // 4
        v = c.counts[i]
        return [v & 0xFF, v >> 8 & 0xFF, v >> 16 & 0xFF, v >> 24 & 0xFF][:length]
