/* One state object with a version counter, and a command queue (SPEC 3.2 state_bus).
 * UI and webui read snapshots; commands go through the queue to the logic task. */
#pragma once

#include "cu_common.h"
#include "modules.h"

typedef enum {
    ALERT_DI_CARD = 1u << 0,        /* S5 */
    ALERT_OUT_CARD = 1u << 1,       /* S11 */
    ALERT_ROOF_OPEN_DRIVING = 1u << 2,
    ALERT_FRIDGE_SENSOR = 1u << 3,
    ALERT_FRIDGE_FAULT = 1u << 4,
    ALERT_HEATER_FAULT = 1u << 5,
    ALERT_WASTE_FULL = 1u << 6,
    ALERT_UNDERVOLTAGE = 1u << 7,
    ALERT_OUTPUT_CONFLICT = 1u << 8,
} cu_alert_t;

typedef struct {
    uint32_t version;
    cu_ms_t ts;
    struct {
        roof_pos_t pos;
        roof_dir_t active;
        bool auto_run, tap, busy, released;
        roof_block_t block;
    } roof;
    struct {
        bool on, released;
        bool request;        /* thermostat wants cooling */
        bool output;         /* cooling request output actually driven (K3) */
        /* compressor state: unknown, there is no feedback input */
        fridge_thermo_t thermo;
        bool thermo_cal, drive_known;
        float target_c, temp_c, raw_v; /* raw_v: AI1 volts */
        cu_quality_t temp_q;
        cu_ms_t temp_ts;
        uint8_t fault_code;   /* only with an identified diagnostic profile */
        bool diag_decoding;
        uint32_t diag_transitions;
    } fridge;
    struct {
        bool on, level_cal, released;
        uint8_t level, levels;
        heat_phase_t phase;
    } heater;
    struct {
        int8_t fresh_step;
        cu_quality_t fresh_q, waste_q;
        bool waste_full;
    } tanks;
    float outside_c;
    cu_quality_t outside_q;
    struct {
        bool ignition, key86s, dplus, dim58, moving;
        bool di_ok, out_ok;
    } vehicle;
    struct {
        bool connected;
        float v[EN_COUNT];
        cu_quality_t q[EN_COUNT];
    } energy;
    struct {
        uint16_t di;
        uint8_t relays, ladder;
        float ai[CU_N_AI];
    } io;
    uint32_t alerts;
} cu_state_t;

typedef enum { CMD_ROOF = 1, CMD_FRIDGE, CMD_HEATER } cu_cmd_type_t;

typedef struct {
    cu_cmd_type_t type;
    uint32_t seq;      /* roof: rising run-message number */
    roof_dir_t dir;    /* roof */
    bool auto_run;     /* roof: tap */
    bool on;           /* fridge, heater */
    float value;       /* fridge target in degrees C, heater level */
} cu_cmd_t;

#define STATE_BUS_QUEUE 32

typedef struct {
    cu_state_t state;
    cu_cmd_t q[STATE_BUS_QUEUE];
    uint8_t head, tail;
    /* Optional lock for the target (FreeRTOS mutex); NULL on the host. */
    void (*lock)(void *ctx);
    void (*unlock)(void *ctx);
    void *lock_ctx;
} state_bus_t;

void state_bus_init(state_bus_t *b);
void state_bus_publish(state_bus_t *b, const cu_state_t *s); /* bumps the version */
void state_bus_snapshot(state_bus_t *b, cu_state_t *out);
bool state_bus_push(state_bus_t *b, const cu_cmd_t *c); /* false if full */
bool state_bus_pop(state_bus_t *b, cu_cmd_t *out);
