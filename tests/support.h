/* Shared helpers for host tests. */
#pragma once

#include "control.h"
#include "hal_sim.h"
#include "profile.h"
#include "state_bus.h"

/* Released functions, measured-looking calibration: for logic tests only. */
extern const char *const OVERLAY_RELEASED;

char *read_file(const char *path);
/* Loads profiles/t6-california-a8l-2018.json with an optional overlay; asserts success. */
void load_profile(profile_t *p, const char *overlay);

typedef struct {
    profile_t p;
    hal_sim_t sim;
    hal_io_t io;
    state_bus_t bus;
    control_t c;
    cu_ms_t now;
    uint32_t seq;
} rig_t;

void rig_init(rig_t *r, const char *overlay);
/* Advances time in 10 ms ticks: io every 20 ms, logic every 50 ms. */
void rig_run(rig_t *r, cu_ms_t ms);
void rig_roof(rig_t *r, roof_dir_t dir, bool auto_run);
/* Keeps sending run messages every 100 ms for ms. */
void rig_roof_hold(rig_t *r, roof_dir_t dir, bool auto_run, cu_ms_t ms);
void rig_set_di(rig_t *r, uint8_t ch, bool on);
bool rig_relay(const rig_t *r, uint8_t ch);
