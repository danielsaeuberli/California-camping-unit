"""Decoder for the Secop fridge diagnostic output (terminal D, DI3).

The electronics emits ``n`` low-active pulses of ~250 ms, the sequence repeating
every ~4 s. ``n`` is the error code (1-6). A code is reported only after two identical
consecutive sequences; pulses far from the nominal width invalidate the sequence.
"""

from __future__ import annotations


class PulseDecoder:
    def __init__(self, pulse_s: float = 0.25, frame_s: float = 4.0, active_low: bool = True) -> None:
        self.pulse_s = pulse_s
        self.frame_s = frame_s
        self.active_low = active_low
        self.code: int | None = None
        self._last_level: bool | None = None
        self._pulse_start: float | None = None
        self._last_pulse_end: float | None = None
        self._count = 0
        self._valid = True
        self._prev_seq: int | None = None
        self._last_seq_end: float | None = None

    @property
    def _seq_gap_s(self) -> float:
        # Pulses inside a sequence are ~1 pulse width apart; a pause of 4 widths ends it.
        return self.pulse_s * 4

    def update(self, level: bool, now: float) -> int | None:
        active = (not level) if self.active_low else level
        if self._last_level is None:
            self._last_level = active
            return self.code
        if active and not self._last_level:
            self._pulse_start = now
        elif not active and self._last_level and self._pulse_start is not None:
            width = now - self._pulse_start
            self._pulse_start = None
            if width < self.pulse_s * 0.4:
                pass  # glitch, ignore
            elif width > self.pulse_s * 2.0:
                self._valid = False
            else:
                self._count += 1
                self._last_pulse_end = now
        self._last_level = active

        if active and self._pulse_start is not None and now - self._pulse_start > self.frame_s:
            # Line stuck active: not a code.
            self._reset_sequence()
            self._prev_seq = None
            self.code = None
        if (
            not active
            and self._last_pulse_end is not None
            and now - self._last_pulse_end >= self._seq_gap_s
        ):
            self._finish_sequence(now)
        if self._last_seq_end is not None and now - self._last_seq_end > 2.5 * self.frame_s:
            self.code = None
            self._prev_seq = None
            self._last_seq_end = None
        return self.code

    def _finish_sequence(self, now: float) -> None:
        seq = self._count if self._valid and 1 <= self._count <= 6 else None
        if seq is not None and seq == self._prev_seq:
            self.code = seq
        self._prev_seq = seq
        self._last_seq_end = now if seq is not None else self._last_seq_end
        self._reset_sequence()

    def _reset_sequence(self) -> None:
        self._count = 0
        self._valid = True
        self._last_pulse_end = None
