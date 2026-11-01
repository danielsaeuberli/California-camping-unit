#include "hal_sequent.h"

static int na_di(hal_io_t *io, uint16_t *m) { (void)io; (void)m; return -1; }
static int na_ai(hal_io_t *io, float v[CU_N_AI]) { (void)io; (void)v; return -1; }
static int na_edges(hal_io_t *io, uint8_t ch, uint32_t *c) { (void)io; (void)ch; (void)c; return -1; }
static int na_relay(hal_io_t *io, uint8_t ch, bool on) { (void)io; (void)ch; (void)on; return -1; }
static int na_ladder(hal_io_t *io, uint8_t bits) { (void)io; (void)bits; return -1; }

static hal_io_t io = {na_di, na_ai, na_edges, na_relay, na_ladder, NULL};

hal_io_t *hal_sequent_io(void)
{
    return &io; /* TODO(M4): I2C drivers, register maps from the Sequent repositories */
}
