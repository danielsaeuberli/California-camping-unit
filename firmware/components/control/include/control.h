/* Glue between HAL, signals, modules, safety and the state bus (SPEC 3.3).
 * control_io_step is the io task (20 ms): the only place that writes outputs.
 * control_logic_step is the logic task (50 ms): commands and modules.
 * On the target both run in their own task and share the struct under lock/unlock. */
#pragma once

#include "cu_common.h"
#include "hal_io.h"
#include "modules.h"
#include "profile.h"
#include "safety.h"
#include "signals.h"
#include "state_bus.h"

typedef struct {
    const profile_t *p;
    hal_io_t *io;
    state_bus_t *bus;

    safety_t safety;
    roof_t roof;
    fridge_t fridge;
    heater_t heater;
    tanks_t tanks;
    vehicle_t vehicle;
    energy_t energy;
    victron_ctl_t victron;

    sig_debounce_t deb[CU_N_DI];
    uint16_t di;          /* debounced DI1..DI16 */
    float ai[CU_N_AI];    /* volts */
    uint32_t io_cycles;
    bool k_open, k_close; /* roof request in logical directions */
    cu_outputs_t written;
    bool written_valid;
    float fridge_temp_c, outside_c;
    cu_ms_t fridge_temp_ts; /* time of the last valid fridge temperature */
    cu_quality_t fridge_q, outside_q;
    cu_ms_t undervoltage_since;
    bool undervoltage;

    void (*lock)(void *ctx);
    void (*unlock)(void *ctx);
    void *lock_ctx;
} control_t;

/* S1 runs first, before anything else. Returns the number of failed writes. */
int control_init(control_t *c, const profile_t *p, hal_io_t *io, state_bus_t *bus, cu_ms_t now);
void control_io_step(control_t *c, cu_ms_t now);
void control_logic_step(control_t *c, cu_ms_t now);
bool control_di(const control_t *c, uint8_t ch); /* 1-based */
