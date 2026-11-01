/* Raw value -> value with quality (SPEC 3.2 signals). No I/O, time passed in. */
#pragma once

#include "cu_common.h"
#include "profile.h"

/* Digital input debounce: a level counts after n equal samples in a row. */
typedef struct {
    uint8_t n, count;
    bool state, last_raw, valid;
} sig_debounce_t;

void sig_debounce_init(sig_debounce_t *d, uint8_t n);
bool sig_debounce(sig_debounce_t *d, bool raw);

/* Edges of a free-running counter within a sliding window (speed signal, S4). */
#define SIG_EDGE_SLOTS 64
typedef struct {
    uint32_t window_ms;
    cu_ms_t t[SIG_EDGE_SLOTS];
    uint32_t c[SIG_EDGE_SLOTS];
    uint8_t head, n;
} sig_edges_t;

void sig_edges_init(sig_edges_t *e, uint32_t window_ms);
/* Adds a counter sample and returns the edges within the window. */
uint32_t sig_edges_update(sig_edges_t *e, cu_ms_t now, uint32_t counter);

/* Resistance of a sensor between AI and ground with a pull-up to vref.
 * Quality OPEN/SHORT at the rails, UNCALIBRATED without pull-up value. */
cu_quality_t sig_ohm_from_volts(float v, float vref, cu_cal_t pullup, float *ohm);

/* Temperature from three calibration points (Steinhart-Hart). */
cu_quality_t sig_ntc_temp(const profile_ntc_t *ntc, float ohm, float *temp_c);

/* Running median over the last n samples (n <= SIG_MEDIAN_MAX). */
#define SIG_MEDIAN_MAX 64
typedef struct {
    float buf[SIG_MEDIAN_MAX];
    uint8_t n, size, head;
} sig_median_t;

void sig_median_init(sig_median_t *m, uint8_t size);
float sig_median_add(sig_median_t *m, float v);
