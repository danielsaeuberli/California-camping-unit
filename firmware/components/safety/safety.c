#include "safety.h"

#include <string.h>

int safety_boot_all_off(hal_io_t *io)
{
    int failed = 0;
    for (uint8_t ch = 1; ch <= CU_N_RELAYS; ch++) {
        if (io->write_relay(io, ch, false) != 0) {
            failed++;
        }
    }
    if (io->write_ladder(io, 0) != 0) {
        failed++;
    }
    return failed;
}

void safety_init(safety_t *s)
{
    memset(s, 0, sizeof *s);
    s->di_ok = true;
    s->out_ok = true;
}

void safety_note_di(safety_t *s, bool ok)
{
    if (ok) {
        s->di_fail = 0;
        s->di_ok = true;
    } else if (++s->di_fail >= SAFETY_FAIL_LIMIT) {
        s->di_fail = SAFETY_FAIL_LIMIT;
        s->di_ok = false;
    }
}

void safety_note_out(safety_t *s, bool ok)
{
    if (ok) {
        s->out_fail = 0;
        s->out_ok = true;
    } else if (++s->out_fail >= SAFETY_FAIL_LIMIT) {
        s->out_fail = SAFETY_FAIL_LIMIT;
        s->out_ok = false;
    }
}

static void off(cu_outputs_t *o, uint8_t ch)
{
    if (ch >= 1 && ch <= CU_N_RELAYS) {
        o->relay[ch - 1] = false;
    }
}

void safety_filter(safety_t *s, const profile_t *p, cu_outputs_t *o)
{
    /* Relays that belong to no function never switch. */
    for (uint8_t ch = 1; ch <= CU_N_RELAYS; ch++) {
        if (ch != p->k_roof_a && ch != p->k_roof_b && ch != p->k_fridge && ch != p->k_heater) {
            off(o, ch);
        }
    }
    if (p->measure_outputs_locked) {
        memset(o, 0, sizeof *o);
        return;
    }
    /* No confirmed output stage (TODO_MEASURE): never drive the channel. */
    if (p->drive_roof_a == DRIVE_UNKNOWN || p->drive_roof_b == DRIVE_UNKNOWN) {
        off(o, p->k_roof_a);
        off(o, p->k_roof_b);
    }
    if (p->drive_fridge == DRIVE_UNKNOWN) {
        off(o, p->k_fridge);
    }
    if (p->drive_heater == DRIVE_UNKNOWN) {
        off(o, p->k_heater);
    }
    if (!p->release_roof || !s->di_ok || !s->out_ok) {
        off(o, p->k_roof_a);
        off(o, p->k_roof_b);
    }
    if (!p->release_fridge) {
        off(o, p->k_fridge);
    }
    if (!p->release_heater) {
        off(o, p->k_heater);
        o->ladder = 0;
    }
    if (o->relay[p->k_roof_a - 1] && o->relay[p->k_roof_b - 1]) { /* S2 */
        off(o, p->k_roof_a);
        off(o, p->k_roof_b);
        s->conflicts++;
    }
}
