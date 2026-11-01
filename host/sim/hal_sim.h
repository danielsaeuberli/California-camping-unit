/* Simulated I/O for host tests and the simulator. */
#pragma once

#include "hal_io.h"

typedef struct {
    uint16_t di;                 /* DI1..DI16, bit 0 = DI1 */
    float ai[CU_N_AI];           /* volts */
    uint32_t edges[CU_N_DI];     /* rising-edge counters */
    bool relay[CU_N_RELAYS];
    uint8_t ladder;
    bool fail_di, fail_ai, fail_out; /* inject I2C errors */
    /* Write log for tests: every write in order. */
    struct {
        uint8_t ch; /* 1..8 relay, 0 = ladder */
        uint8_t value;
    } log[256];
    uint16_t n_log;
    uint32_t ladder_writes;
    int both_roof_on; /* set if K1 and K2 were ever on together */
} hal_sim_t;

void hal_sim_init(hal_sim_t *s, hal_io_t *io);
