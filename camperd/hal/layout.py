"""Channel layout of the two SM-H-001 cards (HARDWARE.md 4).

Card A carries K1-K8, DI1-DI8 and AI1-AI8; card B carries K9-K16, DI9-DI16 and AI9-AI16.
"""

from __future__ import annotations

PER_CARD = 8
CARDS = ("sm_h001_a", "sm_h001_b")
N_RELAY = N_DI = N_AI = PER_CARD * len(CARDS)


def card_of(ch: int) -> tuple[str, int]:
    """1-based unit channel -> (card key, 1-based channel on that card)."""
    if not 1 <= ch <= N_RELAY:
        raise ValueError(f"channel {ch} out of range")
    idx, local = divmod(ch - 1, PER_CARD)
    return CARDS[idx], local + 1


def card_index(ch: int) -> int:
    """0 for card A, 1 for card B."""
    return (ch - 1) // PER_CARD
