#include "hal_sim.h"

#include <math.h>
#include <string.h>

static hal_sim_t *sim(hal_io_t *io)
{
    return (hal_sim_t *)io->ctx;
}

static int read_di(hal_io_t *io, uint16_t *mask)
{
    if (sim(io)->fail_di) {
        return -1;
    }
    *mask = sim(io)->di;
    return 0;
}

static int read_ai(hal_io_t *io, float v[CU_N_AI])
{
    if (sim(io)->fail_ai) {
        return -1;
    }
    memcpy(v, sim(io)->ai, sizeof(float) * CU_N_AI);
    return 0;
}

static int read_edges(hal_io_t *io, uint8_t ch, uint32_t *count)
{
    if (sim(io)->fail_di || ch < 1 || ch > CU_N_DI) {
        return -1;
    }
    *count = sim(io)->edges[ch - 1];
    return 0;
}

static void log_write(hal_sim_t *s, uint8_t ch, uint8_t v)
{
    if (s->n_log < CU_ARRAY_LEN(s->log)) {
        s->log[s->n_log].ch = ch;
        s->log[s->n_log].value = v;
        s->n_log++;
    }
}

static int write_relay(hal_io_t *io, uint8_t ch, bool on)
{
    hal_sim_t *s = sim(io);
    if (s->fail_out || ch < 1 || ch > CU_N_RELAYS) {
        return -1;
    }
    s->relay[ch - 1] = on;
    log_write(s, ch, on);
    if (s->relay[0] && s->relay[1]) {
        s->both_roof_on = 1;
    }
    return 0;
}

static int write_ladder(hal_io_t *io, uint8_t bits)
{
    hal_sim_t *s = sim(io);
    if (s->fail_out) {
        return -1;
    }
    s->ladder = bits & 0x0f;
    s->ladder_writes++;
    log_write(s, 0, bits);
    return 0;
}

void hal_sim_init(hal_sim_t *s, hal_io_t *io)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < CU_N_AI; i++) {
        s->ai[i] = NAN;
    }
    /* The card relays start in an unknown state (they keep it over a reset, SPEC 4.2). */
    for (int i = 0; i < CU_N_RELAYS; i++) {
        s->relay[i] = true;
    }
    s->ladder = 0x0f;
    *io = (hal_io_t){read_di, read_ai, read_edges, write_relay, write_ladder, s};
}
