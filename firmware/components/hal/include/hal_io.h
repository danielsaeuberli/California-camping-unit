/* Hardware interface for inputs and outputs (SPEC 3.2 hal). Implementations:
 * hal_sim (host), hal_sequent (SM-H-001 + SM-I-009 over I2C 0), hal_ladder (GPIO H1-H4). */
#pragma once

#include "cu_common.h"

typedef struct hal_io hal_io_t;

struct hal_io {
    /* DI1..DI16 as bit mask, bit 0 = DI1. Returns 0 on success. */
    int (*read_di)(hal_io_t *io, uint16_t *mask);
    /* AI1..AI8 in volts. Returns 0 on success. */
    int (*read_ai)(hal_io_t *io, float volts[CU_N_AI]);
    /* Rising-edge counter of one DI channel (1-based), free running. */
    int (*read_edges)(hal_io_t *io, uint8_t ch, uint32_t *count);
    /* Relay 1..8 of the SM-H-001. */
    int (*write_relay)(hal_io_t *io, uint8_t ch, bool on);
    /* H1..H4 as bits, bit 0 = H1. 0 = all segments bridged (base resistor). */
    int (*write_ladder)(hal_io_t *io, uint8_t bits);
    void *ctx;
};
