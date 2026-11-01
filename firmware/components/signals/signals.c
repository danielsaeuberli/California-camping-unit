#include "signals.h"

#include <math.h>
#include <string.h>

void sig_debounce_init(sig_debounce_t *d, uint8_t n)
{
    memset(d, 0, sizeof *d);
    d->n = n ? n : 1;
}

bool sig_debounce(sig_debounce_t *d, bool raw)
{
    if (!d->valid || raw != d->last_raw) {
        d->last_raw = raw;
        d->count = 1;
        d->valid = true;
    } else if (d->count < d->n) {
        d->count++;
    }
    if (d->count >= d->n) {
        d->state = raw;
    }
    return d->state;
}

void sig_edges_init(sig_edges_t *e, uint32_t window_ms)
{
    memset(e, 0, sizeof *e);
    e->window_ms = window_ms;
}

uint32_t sig_edges_update(sig_edges_t *e, cu_ms_t now, uint32_t counter)
{
    e->t[e->head] = now;
    e->c[e->head] = counter;
    e->head = (uint8_t)((e->head + 1) % SIG_EDGE_SLOTS);
    if (e->n < SIG_EDGE_SLOTS) {
        e->n++;
    }
    /* Oldest sample still inside the window (or the oldest we have). */
    uint32_t base = counter;
    for (uint8_t i = 0; i < e->n; i++) {
        uint8_t idx = (uint8_t)((e->head + SIG_EDGE_SLOTS - 1 - i) % SIG_EDGE_SLOTS);
        if (now - e->t[idx] > (cu_ms_t)e->window_ms) {
            break;
        }
        base = e->c[idx];
    }
    return counter - base; /* unsigned: wraps correctly */
}

cu_quality_t sig_ohm_from_volts(float v, float vref, cu_cal_t pullup, float *ohm)
{
    *ohm = NAN;
    if (!pullup.cal) {
        return CU_Q_UNCALIBRATED;
    }
    if (isnan(v) || isnan(vref) || vref <= 0.5f) {
        return CU_Q_STALE;
    }
    if (v >= vref * 0.98f) {
        return CU_Q_OPEN;
    }
    if (v <= vref * 0.01f) {
        return CU_Q_SHORT;
    }
    *ohm = pullup.v * v / (vref - v);
    return CU_Q_OK;
}

/* Solves the Steinhart-Hart coefficients for three points. */
static bool sh_coeffs(const profile_ntc_t *n, double *a, double *b, double *c)
{
    double l1 = log(n->ohm[0]), l2 = log(n->ohm[1]), l3 = log(n->ohm[2]);
    double y1 = 1.0 / (n->temp_c[0] + 273.15), y2 = 1.0 / (n->temp_c[1] + 273.15), y3 = 1.0 / (n->temp_c[2] + 273.15);
    if (l1 == l2 || l2 == l3 || l1 == l3) {
        return false;
    }
    double g2 = (y2 - y1) / (l2 - l1), g3 = (y3 - y1) / (l3 - l1);
    *c = ((g3 - g2) / (l3 - l2)) / (l1 + l2 + l3);
    *b = g2 - *c * (l1 * l1 + l1 * l2 + l2 * l2);
    *a = y1 - (*b + l1 * l1 * *c) * l1;
    return true;
}

cu_quality_t sig_ntc_temp(const profile_ntc_t *ntc, float ohm, float *temp_c)
{
    *temp_c = NAN;
    if (!ntc->cal) {
        return CU_Q_UNCALIBRATED;
    }
    double a, b, c;
    if (!sh_coeffs(ntc, &a, &b, &c) || !(ohm > 0)) {
        return CU_Q_IMPLAUSIBLE;
    }
    double l = log(ohm);
    double t = 1.0 / (a + b * l + c * l * l * l) - 273.15;
    if (t < -50 || t > 120) {
        return CU_Q_IMPLAUSIBLE;
    }
    *temp_c = (float)t;
    return CU_Q_OK;
}

void sig_median_init(sig_median_t *m, uint8_t size)
{
    memset(m, 0, sizeof *m);
    m->size = size == 0 ? 1 : (size > SIG_MEDIAN_MAX ? SIG_MEDIAN_MAX : size);
}

float sig_median_add(sig_median_t *m, float v)
{
    m->buf[m->head] = v;
    m->head = (uint8_t)((m->head + 1) % m->size);
    if (m->n < m->size) {
        m->n++;
    }
    float s[SIG_MEDIAN_MAX];
    memcpy(s, m->buf, sizeof(float) * m->n);
    for (uint8_t i = 1; i < m->n; i++) { /* insertion sort, n <= 64 */
        float x = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > x) {
            s[j + 1] = s[j];
            j--;
        }
        s[j + 1] = x;
    }
    return s[m->n / 2];
}
