/* State machines (SPEC 7). Pure C: no I/O, no FreeRTOS, time passed in. */
#pragma once

#include "cu_common.h"
#include "profile.h"
#include "signals.h"

/* ---------------- Roof (7.1, S2-S5, S11) ---------------- */

typedef enum { ROOF_STOP = 0, ROOF_OPEN, ROOF_CLOSE } roof_dir_t;

typedef enum {
    ROOF_POS_UNKNOWN = 0, /* no truth table (O6) or an undefined combination */
    ROOF_POS_CLOSED,
    ROOF_POS_OPEN,
    ROOF_POS_BETWEEN,
} roof_pos_t;

typedef enum {
    ROOF_OK = 0,
    ROOF_BLOCK_NOT_RELEASED,  /* release.roof false (measurement pending) */
    ROOF_BLOCK_LOCKED,        /* measure.outputs_locked */
    ROOF_BLOCK_DIRECTION,     /* K1/K2 meaning not measured (O6) */
    ROOF_BLOCK_DRIVE,         /* output stage of K1/K2 not confirmed */
    ROOF_BLOCK_IGNITION,      /* require_ignition and ignition off */
    ROOF_BLOCK_MOVING,        /* S4: opening while the vehicle moves */
    ROOF_BLOCK_INPUTS,        /* S5: SM-I-009 failed */
    ROOF_BLOCK_OUTPUTS,       /* S11: SM-H-001 failed */
    ROOF_BLOCK_NO_CONFIRM,    /* S3: no run message for hold_timeout */
    ROOF_BLOCK_MAX_RUN,       /* S3: max run time */
    ROOF_BLOCK_AT_END,        /* tap: already in that end position */
} roof_block_t;

typedef struct {
    bool ignition, moving, di_ok, out_ok, released, locked;
    bool fb_a, fb_b; /* DI1, DI2 */
} roof_in_t;

typedef struct {
    const profile_t *p;
    roof_dir_t want;      /* requested by the latest run message */
    bool auto_run;        /* tap: run to the end position */
    uint32_t last_seq;
    cu_ms_t last_msg;
    roof_dir_t active;    /* energized direction */
    roof_dir_t last_dir;  /* direction of the previous run */
    cu_ms_t run_start, off_since, afterrun_until;
    roof_block_t block;   /* reason of the last refusal or stop */
    bool latched;         /* stopped by a lock: wait for a new press (gap > hold_timeout) */
    roof_pos_t pos;
} roof_t;

/* True if tapping may start a run to the end position: needs the measured truth
 * table and K1/K2 meaning, and roof.tap_to_run. Otherwise hold-to-run. */
bool roof_tap_available(const profile_t *p);
void roof_init(roof_t *r, const profile_t *p, cu_ms_t now);
/* Run message from the UI every 100 ms (dir ROOF_STOP = stop now). seq must rise. */
roof_block_t roof_command(roof_t *r, roof_dir_t dir, uint32_t seq, bool auto_run, cu_ms_t now, const roof_in_t *in);
/* Evaluates timeouts and locks; outputs the logical directions (never both). */
void roof_step(roof_t *r, cu_ms_t now, const roof_in_t *in, bool *k_open, bool *k_close);
/* Running or pump still running after a run (S8). */
bool roof_busy(const roof_t *r, cu_ms_t now);
roof_pos_t roof_position(const profile_t *p, bool fb_a, bool fb_b);

/* ---------------- Fridge (7.2, S6) ----------------
 * Separate parts: temperature (control.c), thermostat (fridge_step), cooling request
 * output (control.c, only with a confirmed drive), diagnostics (raw recorder + optional
 * decoder). There is no compressor feedback, so the compressor state is always unknown. */

typedef enum {
    FRIDGE_OFF = 0,          /* switched off by the user */
    FRIDGE_UNCALIBRATED,     /* set point range or hysteresis not documented (TODO_MEASURE) */
    FRIDGE_SENSOR_FAULT,     /* G817 not ok (open, short, stale, uncalibrated) */
    FRIDGE_IDLE,             /* on, no cooling demand */
    FRIDGE_DEMAND,           /* on, cooling requested */
    FRIDGE_PROTECT_PAUSE,    /* demand, but the S6 minimum pause still runs */
    FRIDGE_PROTECT_RUN,      /* no demand, but the S6 minimum run still runs */
} fridge_thermo_t;

#define DIAG_RAW 32
typedef struct {
    bool level, valid;               /* last sampled level (true = high) */
    cu_ms_t since;                   /* time of the last transition */
    struct {
        bool level;                  /* level that ended */
        uint16_t ms;                 /* its duration, saturated */
    } raw[DIAG_RAW];                 /* last transitions, ring buffer (always recorded) */
    uint8_t raw_head, raw_n;
    uint32_t transitions;
    /* Decoder state (only with fridge.diag_decoder set and a measured polarity). */
    uint8_t pulses, pending, code;
    cu_ms_t last_burst;
} fridge_diag_t;

typedef struct {
    const profile_t *p;
    bool on;
    float target_c;     /* NAN while the set point range is not calibrated */
    bool request;       /* thermostat wants cooling (not the same as the output, not the compressor) */
    cu_ms_t last_change;
    fridge_thermo_t thermo;
    fridge_diag_t diag;
} fridge_t;

bool fridge_thermostat_calibrated(const profile_t *p);
void fridge_init(fridge_t *f, const profile_t *p, cu_ms_t now);
/* Returns false if the set point cannot be set (range not calibrated). */
bool fridge_set(fridge_t *f, bool on, float target_c);
void fridge_step(fridge_t *f, cu_ms_t now, cu_quality_t temp_q, float temp_c);
/* Diagnostic line T32b/9, sampled every io cycle (high = true). Always recorded raw;
 * decoded only with an identified profile (fridge.diag_decoder, diag_polarity). */
void fridge_diag_sample(fridge_diag_t *d, const profile_t *p, cu_ms_t now, bool high);

/* ---------------- Heater (7.3, S7) ---------------- */

typedef enum { HEAT_OFF = 0, HEAT_START, HEAT_HEATING, HEAT_AFTERRUN, HEAT_FAULT } heat_phase_t;

typedef struct {
    const profile_t *p;
    bool on;
    uint8_t level;
    heat_phase_t phase;
    cu_ms_t phase_since;
    uint8_t bits;        /* H1-H4 as currently switched */
    cu_ms_t last_bit_change;
} heater_t;

void heater_init(heater_t *h, const profile_t *p, cu_ms_t now);
/* Returns false if the level cannot be set (ladder not calibrated or out of range). */
bool heater_set(heater_t *h, bool on, uint8_t level);
/* allowed = released and not locked. run = operating indicator T6ap/2. */
void heater_step(heater_t *h, cu_ms_t now, bool run, bool allowed, bool *s_plus, uint8_t *ladder);

/* ---------------- Tanks (7.4) ---------------- */

typedef struct {
    const profile_t *p;
    sig_median_t fresh_med, waste_med;
    int8_t fresh_step; /* 0..3 (empty, 1/3, 2/3, full), -1 unknown */
    cu_quality_t fresh_q;
    bool waste_full;
    cu_quality_t waste_q;
} tanks_t;

void tanks_init(tanks_t *t, const profile_t *p);
void tanks_update(tanks_t *t, float fresh_v, float waste_v, float vref);

/* ---------------- Vehicle (7.6) ---------------- */

typedef struct {
    const profile_t *p;
    sig_edges_t speed;
    uint32_t speed_edges;
    bool moving;
} vehicle_t;

void vehicle_init(vehicle_t *v, const profile_t *p);
/* Speed from the edge counter of DI6, window profile speed.window_ms. Without a
 * measured threshold any edge counts as moving (blocks opening rather too often). */
bool vehicle_update_speed(vehicle_t *v, cu_ms_t now, uint32_t counter);

/* ---------------- Energy (7.7, Cerbo values, read only) ---------------- */

typedef enum {
    EN_SOC = 0,
    EN_BATTERY_V,
    EN_BATTERY_A,
    EN_BATTERY_W,
    EN_PV_W,
    EN_CHARGER_W,
    EN_ALT_W,        /* system/0/Dc/Alternator/Power (Orion XS, confirmed in v1, O21) */
    EN_VEBUS_STATE,
    EN_VEBUS_MODE,
    EN_ACTIVE_INPUT,
    EN_IN_V,
    EN_IN_P,
    EN_OUT_P,
    EN_OUT_S,
    EN_ALARM_OVERLOAD,
    EN_COUNT,
} energy_key_t;

typedef struct {
    float v[EN_COUNT];
    cu_ms_t t[EN_COUNT];
    bool has[EN_COUNT];
    uint32_t stale_ms;
    bool connected;
} energy_t;

void energy_init(energy_t *e, uint32_t stale_ms);
void energy_set(energy_t *e, energy_key_t k, float v, cu_ms_t now);
/* Value if present and not older than stale_ms; quality STALE otherwise. */
cu_quality_t energy_get(const energy_t *e, energy_key_t k, cu_ms_t now, float *v);
