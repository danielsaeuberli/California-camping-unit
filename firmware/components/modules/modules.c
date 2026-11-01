#include "modules.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------------- Roof ---------------- */

bool roof_tap_available(const profile_t *p)
{
    return p->roof_tap_to_run && p->roof_table_cal && p->roof_a_meaning != ROOF_MEANING_UNKNOWN &&
           p->roof_b_meaning != ROOF_MEANING_UNKNOWN;
}

roof_pos_t roof_position(const profile_t *p, bool a, bool b)
{
    if (!p->roof_table_cal) {
        return ROOF_POS_UNKNOWN;
    }
    uint8_t ia = a ? 1 : 0, ib = b ? 1 : 0;
    if (p->roof_closed[0] == ia && p->roof_closed[1] == ib) {
        return ROOF_POS_CLOSED;
    }
    if (p->roof_open[0] == ia && p->roof_open[1] == ib) {
        return ROOF_POS_OPEN;
    }
    if (p->roof_between[0] == ia && p->roof_between[1] == ib) {
        return ROOF_POS_BETWEEN;
    }
    return ROOF_POS_UNKNOWN;
}

void roof_init(roof_t *r, const profile_t *p, cu_ms_t now)
{
    memset(r, 0, sizeof *r);
    r->p = p;
    r->off_since = now; /* S2: the gap also applies right after start (S1 switched off) */
}

/* Lock that forbids running in dir right now (ROOF_OK if none). */
static roof_block_t lock_for(const roof_t *r, roof_dir_t dir, const roof_in_t *in)
{
    const profile_t *p = r->p;
    if (!in->released) {
        return ROOF_BLOCK_NOT_RELEASED;
    }
    if (in->locked) {
        return ROOF_BLOCK_LOCKED;
    }
    if (p->roof_a_meaning == ROOF_MEANING_UNKNOWN || p->roof_b_meaning == ROOF_MEANING_UNKNOWN) {
        return ROOF_BLOCK_DIRECTION; /* OPEN: O6 */
    }
    if (p->drive_roof_a == DRIVE_UNKNOWN || p->drive_roof_b == DRIVE_UNKNOWN) {
        return ROOF_BLOCK_DRIVE; /* output stage not measured (O6) */
    }
    if (!in->out_ok) {
        return ROOF_BLOCK_OUTPUTS;
    }
    if (!in->di_ok) {
        return ROOF_BLOCK_INPUTS;
    }
    if (p->roof_require_ignition && !in->ignition) {
        return ROOF_BLOCK_IGNITION;
    }
    if (dir == ROOF_OPEN && in->moving) {
        return ROOF_BLOCK_MOVING;
    }
    return ROOF_OK;
}

static void stop(roof_t *r, cu_ms_t now, roof_block_t why)
{
    if (r->active != ROOF_STOP) {
        r->last_dir = r->active;
        r->off_since = now;
        if (r->p->roof_pump_afterrun_ms.cal) {
            r->afterrun_until = now + (cu_ms_t)r->p->roof_pump_afterrun_ms.v;
        }
    }
    r->active = ROOF_STOP;
    r->want = ROOF_STOP;
    r->auto_run = false;
    r->block = why;
    /* A stop by a lock or timeout needs a new press: run messages that keep coming
     * (finger still on the key, tap run still confirmed) must not restart the roof. */
    r->latched = why != ROOF_OK && why != ROOF_BLOCK_AT_END;
}

roof_block_t roof_command(roof_t *r, roof_dir_t dir, uint32_t seq, bool auto_run, cu_ms_t now, const roof_in_t *in)
{
    if (dir == ROOF_STOP) {
        stop(r, now, ROOF_OK);
        r->last_seq = seq > r->last_seq ? seq : r->last_seq;
        return ROOF_OK;
    }
    if (seq <= r->last_seq) {
        return r->block; /* old or repeated message: ignore (no replay) */
    }
    r->last_seq = seq;
    if (r->latched) {
        bool new_press = now - r->last_msg > (cu_ms_t)r->p->roof_hold_timeout_ms;
        r->last_msg = now;
        if (!new_press) {
            return r->block;
        }
        r->latched = false;
    }
    roof_block_t lock = lock_for(r, dir, in);
    if (lock != ROOF_OK) {
        stop(r, now, lock);
        r->last_msg = now;
        return lock;
    }
    auto_run = auto_run && roof_tap_available(r->p);
    roof_pos_t pos = roof_position(r->p, in->fb_a, in->fb_b);
    if (auto_run && r->active == ROOF_STOP &&
        ((dir == ROOF_OPEN && pos == ROOF_POS_OPEN) || (dir == ROOF_CLOSE && pos == ROOF_POS_CLOSED))) {
        r->block = ROOF_BLOCK_AT_END;
        return ROOF_BLOCK_AT_END;
    }
    if (r->active != ROOF_STOP && r->active != dir) {
        stop(r, now, ROOF_OK); /* reversing: off first, start after the gap (S2) */
    }
    r->want = dir;
    r->auto_run = auto_run;
    r->last_msg = now;
    r->block = ROOF_OK;
    return ROOF_OK;
}

void roof_step(roof_t *r, cu_ms_t now, const roof_in_t *in, bool *k_open, bool *k_close)
{
    const profile_t *p = r->p;
    r->pos = roof_position(p, in->fb_a, in->fb_b);

    if (r->active != ROOF_STOP) {
        roof_block_t lock = lock_for(r, r->active, in);
        if (lock != ROOF_OK) {
            stop(r, now, lock);
        } else if (now - r->last_msg > (cu_ms_t)p->roof_hold_timeout_ms) {
            stop(r, now, ROOF_BLOCK_NO_CONFIRM); /* S3 */
        } else if (now - r->run_start >= (cu_ms_t)p->roof_max_run_ms) {
            stop(r, now, ROOF_BLOCK_MAX_RUN); /* S3 */
        } else if (r->auto_run && ((r->active == ROOF_OPEN && r->pos == ROOF_POS_OPEN) ||
                                   (r->active == ROOF_CLOSE && r->pos == ROOF_POS_CLOSED))) {
            stop(r, now, ROOF_OK); /* end position reached */
        }
    } else if (r->want != ROOF_STOP) {
        roof_block_t lock = lock_for(r, r->want, in);
        if (lock != ROOF_OK) {
            stop(r, now, lock);
        } else if (now - r->last_msg > (cu_ms_t)p->roof_hold_timeout_ms) {
            stop(r, now, ROOF_BLOCK_NO_CONFIRM);
        } else {
            /* S2: both off for the gap; reversing also waits for the pump run-on. */
            cu_ms_t wait = p->roof_reverse_gap_ms;
            if (r->want != r->last_dir && r->afterrun_until - r->off_since > wait) {
                wait = r->afterrun_until - r->off_since;
            }
            if (now - r->off_since >= wait) {
                r->active = r->want;
                r->run_start = now;
            }
        }
    }
    *k_open = r->active == ROOF_OPEN;
    *k_close = r->active == ROOF_CLOSE;
}

bool roof_busy(const roof_t *r, cu_ms_t now)
{
    return r->active != ROOF_STOP || now < r->afterrun_until;
}

/* ---------------- Fridge ---------------- */

bool fridge_thermostat_calibrated(const profile_t *p)
{
    return p->fridge_target_default_c.cal && p->fridge_target_min_c.cal && p->fridge_target_max_c.cal &&
           p->fridge_on_above_c.cal && p->fridge_off_below_c.cal;
}

void fridge_init(fridge_t *f, const profile_t *p, cu_ms_t now)
{
    memset(f, 0, sizeof *f);
    f->p = p;
    f->target_c = p->fridge_target_default_c.cal ? p->fridge_target_default_c.v : NAN;
    f->last_change = now; /* compressor state unknown after a restart: respect the pause (S6) */
    f->diag.last_burst = now;
    f->diag.since = now;
}

bool fridge_set(fridge_t *f, bool on, float target_c)
{
    const profile_t *p = f->p;
    f->on = on;
    if (!p->fridge_target_min_c.cal || !p->fridge_target_max_c.cal || isnan(target_c)) {
        return isnan(target_c) || target_c == f->target_c;
    }
    if (target_c < p->fridge_target_min_c.v) {
        target_c = p->fridge_target_min_c.v;
    }
    if (target_c > p->fridge_target_max_c.v) {
        target_c = p->fridge_target_max_c.v;
    }
    f->target_c = target_c;
    return true;
}

void fridge_step(fridge_t *f, cu_ms_t now, cu_quality_t temp_q, float temp_c)
{
    const profile_t *p = f->p;
    bool want = f->request;
    if (!f->on) {
        f->thermo = FRIDGE_OFF;
        want = false;
    } else if (!fridge_thermostat_calibrated(p) || isnan(f->target_c)) {
        f->thermo = FRIDGE_UNCALIBRATED;
        want = false;
    } else if (temp_q != CU_Q_OK) {
        f->thermo = FRIDGE_SENSOR_FAULT;
        want = false;
    } else if (!f->request && temp_c >= f->target_c + p->fridge_on_above_c.v) {
        want = true;
    } else if (f->request && temp_c <= f->target_c - p->fridge_off_below_c.v) {
        want = false;
    }
    if (want != f->request) {
        /* S6: minimum run and pause protect the compressor. */
        cu_ms_t since = now - f->last_change;
        bool hold = f->request ? since < (cu_ms_t)p->fridge_min_run_ms : since < (cu_ms_t)p->fridge_min_pause_ms;
        if (!hold) {
            f->request = want;
            f->last_change = now;
        }
    }
    if (f->on && fridge_thermostat_calibrated(p) && temp_q == CU_Q_OK) {
        if (want == f->request) {
            f->thermo = f->request ? FRIDGE_DEMAND : FRIDGE_IDLE;
        } else {
            f->thermo = f->request ? FRIDGE_PROTECT_RUN : FRIDGE_PROTECT_PAUSE;
        }
    }
}

/* Hypothesis only (generic Secop LED code, not confirmed for this vehicle): code n =
 * n active pulses of about 0.25 s, repeated about every 4 s. A code counts after two
 * equal bursts; no burst for 10 s clears it. Active only with diag_decoder set. */
#define DIAG_PULSE_MIN_MS 150
#define DIAG_PULSE_MAX_MS 400
#define DIAG_BURST_GAP_MS 1500
#define DIAG_CLEAR_MS 10000

static void decode(fridge_diag_t *d, cu_ms_t now, bool active_ended, cu_ms_t width, bool idle)
{
    if (active_ended) {
        if (width >= DIAG_PULSE_MIN_MS && width <= DIAG_PULSE_MAX_MS) {
            if (d->pulses < 255) {
                d->pulses++;
            }
        } else if (width > DIAG_PULSE_MAX_MS) {
            d->pulses = 0; /* not a code pulse: drop the burst */
        } /* shorter: glitch, ignore */
    }
    if (idle && d->pulses && now - d->since >= DIAG_BURST_GAP_MS) {
        uint8_t n = d->pulses;
        d->pulses = 0;
        d->last_burst = now;
        if (n == d->pending) {
            d->code = n;
        }
        d->pending = n;
    }
    if (idle && d->pulses == 0 && now - d->last_burst > DIAG_CLEAR_MS) {
        d->code = 0;
        d->pending = 0;
    }
}

void fridge_diag_sample(fridge_diag_t *d, const profile_t *p, cu_ms_t now, bool high)
{
    bool changed = d->valid && high != d->level;
    cu_ms_t width = now - d->since;
    if (changed) {
        d->raw[d->raw_head].level = d->level;
        d->raw[d->raw_head].ms = (uint16_t)(width > 65535 ? 65535 : width);
        d->raw_head = (uint8_t)((d->raw_head + 1) % DIAG_RAW);
        if (d->raw_n < DIAG_RAW) {
            d->raw_n++;
        }
        d->transitions++;
    }
    bool was = d->level;
    if (!d->valid || changed) {
        d->since = now;
    }
    d->level = high;
    d->valid = true;
    if (p->fridge_diag_decoder == DIAG_DECODER_NONE || p->fridge_diag_polarity == DIAG_POL_UNKNOWN) {
        return; /* raw recording only */
    }
    bool active_high = p->fridge_diag_polarity == DIAG_POL_HIGH;
    bool active_ended = changed && was == active_high;
    bool idle = high != active_high;
    decode(d, now, active_ended, width, idle);
}

/* ---------------- Heater ---------------- */

void heater_init(heater_t *h, const profile_t *p, cu_ms_t now)
{
    memset(h, 0, sizeof *h);
    h->p = p;
    h->level = p->heater_level_default;
    h->phase_since = now;
    h->last_bit_change = now;
}

bool heater_set(heater_t *h, bool on, uint8_t level)
{
    h->on = on;
    if (level == h->level) {
        return true;
    }
    if (!h->p->heater_bits_cal || level >= h->p->heater_levels) {
        return false; /* ladder not calibrated: level stays (O7) */
    }
    h->level = level;
    return true;
}

static void phase(heater_t *h, heat_phase_t ph, cu_ms_t now)
{
    if (h->phase != ph) {
        h->phase = ph;
        h->phase_since = now;
    }
}

void heater_step(heater_t *h, cu_ms_t now, bool run, bool allowed, bool *s_plus, uint8_t *ladder)
{
    bool on = h->on && allowed;
    switch (h->phase) {
    case HEAT_OFF:
        if (on) {
            phase(h, HEAT_START, now);
        } else if (run) {
            phase(h, HEAT_AFTERRUN, now); /* still cooling down, e.g. after a restart */
        }
        break;
    case HEAT_START:
        if (!on) {
            phase(h, run ? HEAT_AFTERRUN : HEAT_OFF, now);
        } else if (run) {
            phase(h, HEAT_HEATING, now);
        } else if (now - h->phase_since >= (cu_ms_t)h->p->heater_start_timeout_ms) {
            phase(h, HEAT_FAULT, now);
        }
        break;
    case HEAT_HEATING:
        if (!on) {
            phase(h, HEAT_AFTERRUN, now);
        }
        break;
    case HEAT_AFTERRUN:
        if (!run) {
            phase(h, on ? HEAT_START : HEAT_OFF, now);
        }
        break;
    case HEAT_FAULT:
        if (!h->on) {
            phase(h, run ? HEAT_AFTERRUN : HEAT_OFF, now);
        }
        break;
    }
    *s_plus = on && (h->phase == HEAT_START || h->phase == HEAT_HEATING);

    /* S7: switch the ladder one relay at a time, ladder_gap_ms apart. */
    uint8_t target = (allowed && h->p->heater_bits_cal) ? h->p->heater_level_bits[h->level] : 0;
    if (h->bits != target && now - h->last_bit_change >= (cu_ms_t)h->p->heater_ladder_gap_ms) {
        uint8_t diff = (uint8_t)(h->bits ^ target);
        uint8_t bit = (uint8_t)(diff & (uint8_t)(-diff)); /* lowest differing bit */
        h->bits ^= bit;
        h->last_bit_change = now;
    }
    *ladder = h->bits;
}

/* ---------------- Tanks ---------------- */

void tanks_init(tanks_t *t, const profile_t *p)
{
    memset(t, 0, sizeof *t);
    t->p = p;
    sig_median_init(&t->fresh_med, 50); /* 5 s at 100 ms */
    sig_median_init(&t->waste_med, 50);
    t->fresh_step = -1;
    t->fresh_q = CU_Q_STALE;
    t->waste_q = CU_Q_STALE;
}

void tanks_update(tanks_t *t, float fresh_v, float waste_v, float vref)
{
    const profile_t *p = t->p;
    float ohm;
    cu_quality_t q = sig_ohm_from_volts(fresh_v, vref, p->pullup_fresh, &ohm);
    if (q == CU_Q_OK && !p->g120_cal) {
        q = CU_Q_UNCALIBRATED;
    }
    t->fresh_q = q;
    t->fresh_step = -1;
    if (q == CU_Q_OK) {
        float r = sig_median_add(&t->fresh_med, ohm);
        /* Boundaries empty|1/3, 1/3|2/3, 2/3|full; descending = less resistance when fuller. */
        bool desc = p->g120_steps_ohm[0] > p->g120_steps_ohm[2];
        int8_t step = 0;
        for (int i = 0; i < 3; i++) {
            if (desc ? r < p->g120_steps_ohm[i] : r > p->g120_steps_ohm[i]) {
                step = (int8_t)(i + 1);
            }
        }
        t->fresh_step = step;
    }

    q = sig_ohm_from_volts(waste_v, vref, p->pullup_waste, &ohm);
    if (q == CU_Q_OK && !p->g126_full_below_ohm.cal) {
        q = CU_Q_UNCALIBRATED;
    }
    t->waste_q = q;
    if (q == CU_Q_OK) {
        t->waste_full = sig_median_add(&t->waste_med, ohm) < p->g126_full_below_ohm.v;
    } else {
        t->waste_full = false;
    }
}

/* ---------------- Vehicle ---------------- */

void vehicle_init(vehicle_t *v, const profile_t *p)
{
    memset(v, 0, sizeof *v);
    v->p = p;
    sig_edges_init(&v->speed, p->speed_window_ms);
}

bool vehicle_update_speed(vehicle_t *v, cu_ms_t now, uint32_t counter)
{
    v->speed_edges = sig_edges_update(&v->speed, now, counter);
    uint32_t min = v->p->speed_moving_min_edges.cal ? (uint32_t)v->p->speed_moving_min_edges.v : 1;
    v->moving = v->speed_edges >= (min ? min : 1);
    return v->moving;
}

/* ---------------- Energy ---------------- */

void energy_init(energy_t *e, uint32_t stale_ms)
{
    memset(e, 0, sizeof *e);
    e->stale_ms = stale_ms;
}

void energy_set(energy_t *e, energy_key_t k, float v, cu_ms_t now)
{
    if (k < EN_COUNT && !isnan(v)) {
        e->v[k] = v;
        e->t[k] = now;
        e->has[k] = true;
    }
}

cu_quality_t energy_get(const energy_t *e, energy_key_t k, cu_ms_t now, float *v)
{
    *v = NAN;
    if (k >= EN_COUNT || !e->has[k] || !e->connected || now - e->t[k] > (cu_ms_t)e->stale_ms) {
        return CU_Q_STALE;
    }
    *v = e->v[k];
    return CU_Q_OK;
}

/* ---------------- MultiPlus control ---------------- */

void victron_ctl_init(victron_ctl_t *v, const profile_t *p)
{
    memset(v, 0, sizeof *v);
    v->p = p;
    v->unconfirmed = -1;
}

bool victron_limit_enabled(const profile_t *p)
{
    return p->victron_control && p->victron_limit_min_a.cal && p->victron_limit_max_a.cal;
}

static vc_result_t common(victron_ctl_t *v, const energy_t *e, cu_ms_t now)
{
    if (!v->p->victron_control) {
        return VC_DISABLED;
    }
    if (!e->connected) {
        return VC_OFFLINE;
    }
    if (v->written_once && now - v->last_write < (cu_ms_t)v->p->victron_min_interval_ms) {
        return VC_TOO_FAST;
    }
    return VC_OK;
}

static void queue(victron_ctl_t *v, vc_key_t key, float value, cu_ms_t now)
{
    if (v->n_queue < CU_ARRAY_LEN(v->queue)) {
        v->queue[v->n_queue++] = (vc_write_t){key, value};
    }
    v->pending[key] = true;
    v->expected[key] = value;
    v->sent[key] = now;
    v->last_write = now;
    v->written_once = true;
    v->unconfirmed = -1;
}

vc_result_t victron_request_mode(victron_ctl_t *v, const energy_t *e, uint8_t mode, cu_ms_t now)
{
    vc_result_t r = common(v, e, now);
    if (r == VC_OK && (mode < 1 || mode > 4 || !(v->p->victron_modes & (1u << mode)))) {
        r = VC_BAD_MODE;
    }
    if (r == VC_OK) {
        queue(v, VW_MODE, mode, now);
    }
    v->last = r;
    return r;
}

vc_result_t victron_request_limit(victron_ctl_t *v, const energy_t *e, float amps, cu_ms_t now)
{
    vc_result_t r = common(v, e, now);
    float adj;
    if (r == VC_OK && !victron_limit_enabled(v->p)) {
        r = VC_LIMIT_DISABLED;
    } else if (r == VC_OK && (energy_get(e, EN_LIMIT_ADJUSTABLE, now, &adj) != CU_Q_OK || adj != 1)) {
        r = VC_LIMIT_FIXED;
    } else if (r == VC_OK) {
        amps = roundf(amps * 10) / 10;
        if (isnan(amps) || amps < v->p->victron_limit_min_a.v || amps > v->p->victron_limit_max_a.v) {
            r = VC_OUT_OF_RANGE;
        }
    }
    if (r == VC_OK) {
        queue(v, VW_CURRENT_LIMIT, amps, now);
    }
    v->last = r;
    return r;
}

void victron_ctl_tick(victron_ctl_t *v, const energy_t *e, cu_ms_t now)
{
    static const energy_key_t readback[VW_COUNT] = {EN_VEBUS_MODE, EN_CURRENT_LIMIT};
    for (int k = 0; k < VW_COUNT; k++) {
        if (!v->pending[k]) {
            continue;
        }
        float actual;
        if (energy_get(e, readback[k], now, &actual) == CU_Q_OK && fabsf(actual - v->expected[k]) < 0.05f) {
            v->pending[k] = false;
        } else if (now - v->sent[k] > (cu_ms_t)v->p->victron_confirm_ms) {
            v->pending[k] = false;
            v->unconfirmed = (int8_t)k; /* front switch, BMS or panel overrides */
        }
    }
    if (!e->connected) {
        v->n_queue = 0; /* never replay a stale command after a reconnect */
    }
}

bool victron_pop_write(victron_ctl_t *v, vc_write_t *out)
{
    if (!v->n_queue) {
        return false;
    }
    *out = v->queue[0];
    memmove(v->queue, v->queue + 1, sizeof(vc_write_t) * --v->n_queue);
    return true;
}

int victron_write_topic(char *buf, size_t len, const char *portal, const char *instance, vc_key_t key)
{
    static const char *const path[VW_COUNT] = {"Mode", "Ac/In/1/CurrentLimit"};
    if (key >= VW_COUNT || !portal || !instance || !portal[0] || !instance[0]) {
        return -1;
    }
    int n = snprintf(buf, len, "W/%s/vebus/%s/%s", portal, instance, path[key]);
    return n > 0 && (size_t)n < len ? 0 : -1;
}
