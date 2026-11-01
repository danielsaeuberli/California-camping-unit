/* Vehicle profile: pin mapping, thresholds, times and calibration (SPEC 3.2 profile).
 * Loaded from profiles/t6-california-a8l-2018.json. Unknown keys are an error,
 * "TODO_MEASURE" makes a value uncalibrated (never a substitute value). */
#pragma once

#include <stddef.h>

#include "cu_common.h"

typedef enum { ROOF_MEANING_UNKNOWN = 0, ROOF_MEANING_OPEN, ROOF_MEANING_CLOSE } roof_meaning_t;

/* Output stage of a channel. Unknown (TODO_MEASURE) = the channel is never driven. */
typedef enum { DRIVE_UNKNOWN = 0, DRIVE_RELAY_PLUS, DRIVE_RELAY_GROUND, DRIVE_OPEN_DRAIN, DRIVE_DRY_CONTACT } drive_t;

typedef enum { DIAG_POL_UNKNOWN = 0, DIAG_POL_LOW, DIAG_POL_HIGH } diag_polarity_t;
typedef enum { DIAG_DECODER_NONE = 0, DIAG_DECODER_SECOP_HYPOTHESIS } diag_decoder_t;

typedef struct {
    float temp_c[3];
    float ohm[3];
    bool cal;
} profile_ntc_t;

typedef struct {
    /* Channels, 1-based (K1 = 1, DI1 = 1, AI1 = 1). */
    uint8_t k_roof_a, k_roof_b, k_fridge, k_heater;
    roof_meaning_t roof_a_meaning, roof_b_meaning;
    drive_t drive_roof_a, drive_roof_b, drive_fridge, drive_heater;
    uint8_t di_roof_a, di_roof_b, di_fridge_diag, di_heater_run, di_charger_a11;
    uint8_t di_speed, di_dim58, di_key86s, di_dplus, di_ignition;
    uint8_t ai_fridge, ai_fresh, ai_waste, ai_outside, ai_ref, ai_battery;

    cu_cal_t pullup_fridge, pullup_fresh, pullup_waste, pullup_outside;
    cu_cal_t div_ref, div_battery;

    profile_ntc_t g817, g249;
    float g120_steps_ohm[3]; /* fresh water: below [0] = 1/3, below [1] = 2/3, below [2] = full */
    bool g120_cal;
    cu_cal_t g126_full_below_ohm;

    uint32_t roof_hold_timeout_ms, roof_max_run_ms, roof_reverse_gap_ms;
    bool roof_require_ignition;
    bool roof_tap_to_run;           /* tap starts a run to the end position (needs truth table + meaning) */
    cu_cal_t roof_pump_afterrun_ms; /* hydraulic pump keeps running after the command ends (TODO_MEASURE) */
    bool roof_table_cal;
    uint8_t roof_closed[2], roof_open[2], roof_between[2]; /* (DI1, DI2) levels */

    uint32_t speed_window_ms;
    cu_cal_t speed_moving_min_edges;

    /* Thermostat: set point range and hysteresis are not documented for the E153 (TODO_MEASURE). */
    cu_cal_t fridge_target_default_c, fridge_target_min_c, fridge_target_max_c;
    cu_cal_t fridge_on_above_c, fridge_off_below_c;
    uint32_t fridge_min_run_ms, fridge_min_pause_ms; /* S6 compressor protection (SPEC) */
    diag_polarity_t fridge_diag_polarity;
    diag_decoder_t fridge_diag_decoder;

    uint8_t heater_levels, heater_level_default;
    uint32_t heater_start_timeout_ms, heater_ladder_gap_ms;
    cu_cal_t heater_level_ohm[16]; /* target T2p resistance per level (display/expert view) */
    bool heater_bits_cal;
    uint8_t heater_level_bits[16];

    uint32_t power_idle_ms;
    float power_undervoltage_v;
    uint32_t power_undervoltage_ms;

    char cerbo_ssid[33], cerbo_password[65], cerbo_broker[40];
    uint16_t cerbo_port;
    uint32_t cerbo_keepalive_s, cerbo_stale_s;

    bool measure_outputs_locked;
    uint8_t ui_return_min;
    bool release_roof, release_fridge, release_heater;
} profile_t;

/* Parses a profile. On error returns false and writes a message to err. */
bool profile_parse(const char *json, profile_t *out, char *err, size_t err_len);

/* Parses base with an optional overlay merged on top (objects merged, other values
 * replaced, keys starting with "_" ignored). Used by the simulator only. */
bool profile_parse_with_overlay(const char *base, const char *overlay, profile_t *out, char *err, size_t err_len);
