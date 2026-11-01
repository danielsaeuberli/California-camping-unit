#include "control.h"

#include <math.h>
#include <string.h>

#define AI_EVERY 5   /* AI every 5th io cycle = 100 ms */
#define DI_DEBOUNCE 2 /* 2 x 20 ms */
#define REFRESH_EVERY 5 /* rewrite all outputs every 100 ms: detects a failed card (S11) */

static void lock(control_t *c)
{
    if (c->lock) {
        c->lock(c->lock_ctx);
    }
}

static void unlock(control_t *c)
{
    if (c->unlock) {
        c->unlock(c->lock_ctx);
    }
}

bool control_di(const control_t *c, uint8_t ch)
{
    return ch >= 1 && ch <= CU_N_DI && (c->di >> (ch - 1)) & 1u;
}

int control_init(control_t *c, const profile_t *p, hal_io_t *io, state_bus_t *bus, cu_ms_t now)
{
    int failed = safety_boot_all_off(io); /* S1: first action */
    memset(c, 0, sizeof *c);
    c->p = p;
    c->io = io;
    c->bus = bus;
    safety_init(&c->safety);
    if (failed) {
        for (int i = 0; i < SAFETY_FAIL_LIMIT; i++) {
            safety_note_out(&c->safety, false);
        }
    }
    c->written_valid = failed == 0; /* written = all off */
    roof_init(&c->roof, p, now);
    fridge_init(&c->fridge, p, now);
    heater_init(&c->heater, p, now);
    tanks_init(&c->tanks, p);
    vehicle_init(&c->vehicle, p);
    energy_init(&c->energy, p->cerbo_stale_s * 1000u);
    for (int i = 0; i < CU_N_DI; i++) {
        sig_debounce_init(&c->deb[i], DI_DEBOUNCE);
    }
    for (int i = 0; i < CU_N_AI; i++) {
        c->ai[i] = NAN;
    }
    c->fridge_q = c->outside_q = CU_Q_STALE;
    c->fridge_temp_c = c->outside_c = NAN;
    return failed;
}

static roof_in_t roof_inputs(const control_t *c)
{
    const profile_t *p = c->p;
    return (roof_in_t){
        .ignition = control_di(c, p->di_ignition),
        .moving = c->vehicle.moving,
        .di_ok = c->safety.di_ok,
        .out_ok = c->safety.out_ok,
        .released = p->release_roof,
        .locked = p->measure_outputs_locked,
        .fb_a = control_di(c, p->di_roof_a),
        .fb_b = control_di(c, p->di_roof_b),
    };
}

static float ai(const control_t *c, uint8_t ch)
{
    return ch >= 1 && ch <= CU_N_AI ? c->ai[ch - 1] : NAN;
}

/* 3.3-V reference of the analog inputs (ratiometric); OPEN: O17 until measured. */
static float vref(const control_t *c)
{
    const profile_t *p = c->p;
    if (!p->div_ref.cal) {
        return NAN;
    }
    return ai(c, p->ai_ref) * p->div_ref.v;
}

static void read_inputs(control_t *c, cu_ms_t now)
{
    const profile_t *p = c->p;
    uint16_t raw = 0;
    bool ok = c->io->read_di(c->io, &raw) == 0;
    safety_note_di(&c->safety, ok);
    if (ok) {
        uint16_t di = 0;
        for (int i = 0; i < CU_N_DI; i++) {
            if (sig_debounce(&c->deb[i], (raw >> i) & 1u)) {
                di |= (uint16_t)(1u << i);
            }
        }
        c->di = di;
        uint32_t edges = 0;
        if (c->io->read_edges(c->io, p->di_speed, &edges) == 0) {
            vehicle_update_speed(&c->vehicle, now, edges);
        }
        /* Diagnostic line: raw level every 20 ms (recorded; decoded only with a profile). */
        fridge_diag_sample(&c->fridge.diag, p, now, (raw >> (p->di_fridge_diag - 1)) & 1u);
    } else if (!c->safety.di_ok) {
        c->vehicle.moving = true; /* unknown: treat as moving (opening blocked) */
    }

    if (c->io_cycles % AI_EVERY == 0) {
        float v[CU_N_AI];
        if (c->io->read_ai(c->io, v) == 0) {
            memcpy(c->ai, v, sizeof v);
        } else {
            for (int i = 0; i < CU_N_AI; i++) {
                c->ai[i] = NAN;
            }
        }
        float ref = vref(c), ohm;
        c->fridge_q = sig_ohm_from_volts(ai(c, p->ai_fridge), ref, p->pullup_fridge, &ohm);
        if (c->fridge_q == CU_Q_OK) {
            c->fridge_q = sig_ntc_temp(&p->g817, ohm, &c->fridge_temp_c);
        } else if (!p->div_ref.cal && p->pullup_fridge.cal) {
            c->fridge_q = CU_Q_UNCALIBRATED;
        }
        if (c->fridge_q != CU_Q_OK) {
            c->fridge_temp_c = NAN;
        } else {
            c->fridge_temp_ts = now;
        }
        c->outside_q = sig_ohm_from_volts(ai(c, p->ai_outside), ref, p->pullup_outside, &ohm);
        if (c->outside_q == CU_Q_OK) {
            c->outside_q = sig_ntc_temp(&p->g249, ohm, &c->outside_c);
        }
        if (c->outside_q != CU_Q_OK) {
            c->outside_c = NAN;
        }
        tanks_update(&c->tanks, ai(c, p->ai_fresh), ai(c, p->ai_waste), ref);
    }
}

/* Cooling request output: thermostat demand, released, confirmed output stage, no undervoltage. */
static bool fridge_output(const control_t *c)
{
    const profile_t *p = c->p;
    return c->fridge.request && p->release_fridge && p->drive_fridge != DRIVE_UNKNOWN && !c->undervoltage &&
           !p->measure_outputs_locked;
}

static void write_outputs(control_t *c, const cu_outputs_t *want)
{
    bool ok = true;
    bool refresh = c->io_cycles % REFRESH_EVERY == 0;
    for (uint8_t ch = 1; ch <= CU_N_RELAYS; ch++) {
        bool on = want->relay[ch - 1];
        /* While the card is failing, keep trying to switch the roof relays off (S11). */
        bool retry = !c->safety.out_ok && !on;
        if (!c->written_valid || refresh || retry || c->written.relay[ch - 1] != on) {
            if (c->io->write_relay(c->io, ch, on) != 0) {
                ok = false;
                continue;
            }
            c->written.relay[ch - 1] = on;
        }
    }
    if (!c->written_valid || refresh || c->written.ladder != want->ladder) {
        if (c->io->write_ladder(c->io, want->ladder) == 0) {
            c->written.ladder = want->ladder;
        } else {
            ok = false;
        }
    }
    c->written_valid = ok;
    safety_note_out(&c->safety, ok);
}

void control_io_step(control_t *c, cu_ms_t now)
{
    const profile_t *p = c->p;
    lock(c);
    read_inputs(c, now);
    roof_in_t rin = roof_inputs(c);
    roof_step(&c->roof, now, &rin, &c->k_open, &c->k_close); /* S3/S4 checked every 20 ms */

    cu_outputs_t want;
    memset(&want, 0, sizeof want);
    if (p->roof_a_meaning != ROOF_MEANING_UNKNOWN) {
        bool a_open = p->roof_a_meaning == ROOF_MEANING_OPEN;
        want.relay[p->k_roof_a - 1] = a_open ? c->k_open : c->k_close;
        want.relay[p->k_roof_b - 1] = a_open ? c->k_close : c->k_open;
    }
    want.relay[p->k_fridge - 1] = fridge_output(c);
    bool s_plus = false;
    uint8_t ladder = 0;
    heater_step(&c->heater, now, control_di(c, p->di_heater_run),
                p->release_heater && p->drive_heater != DRIVE_UNKNOWN && !p->measure_outputs_locked, &s_plus, &ladder);
    want.relay[p->k_heater - 1] = s_plus;
    want.ladder = ladder;
    safety_filter(&c->safety, p, &want);
    write_outputs(c, &want);
    c->io_cycles++;
    unlock(c);
}

static void apply(control_t *c, const cu_cmd_t *cmd, cu_ms_t now)
{
    switch (cmd->type) {
    case CMD_ROOF: {
        roof_in_t rin = roof_inputs(c);
        roof_command(&c->roof, cmd->dir, cmd->seq, cmd->auto_run, now, &rin);
        break;
    }
    case CMD_FRIDGE:
        fridge_set(&c->fridge, cmd->on, cmd->value);
        break;
    case CMD_HEATER:
        heater_set(&c->heater, cmd->on, (uint8_t)cmd->value);
        break;
    }
}

static void undervoltage(control_t *c, cu_ms_t now)
{
    const profile_t *p = c->p;
    float v;
    /* Cerbo voltage first, AI8 as fallback (only once the divider is calibrated). */
    if (energy_get(&c->energy, EN_BATTERY_V, now, &v) != CU_Q_OK) {
        v = p->div_battery.cal ? ai(c, p->ai_battery) * p->div_battery.v : NAN;
    }
    if (isnan(v) || v >= p->power_undervoltage_v) {
        c->undervoltage_since = 0;
        c->undervoltage = false;
    } else if (c->undervoltage_since == 0) {
        c->undervoltage_since = now;
    } else if (now - c->undervoltage_since >= (cu_ms_t)p->power_undervoltage_ms) {
        c->undervoltage = true;
    }
}

static void publish(control_t *c, cu_ms_t now)
{
    const profile_t *p = c->p;
    cu_state_t s;
    memset(&s, 0, sizeof s);
    s.ts = now;
    s.roof.pos = c->roof.pos;
    s.roof.active = c->roof.active;
    s.roof.auto_run = c->roof.auto_run;
    s.roof.tap = roof_tap_available(p);
    s.roof.busy = roof_busy(&c->roof, now);
    s.roof.released = p->release_roof;
    s.roof.block = c->roof.block;
    s.fridge.on = c->fridge.on;
    s.fridge.request = c->fridge.request;
    s.fridge.output = c->written.relay[p->k_fridge - 1];
    s.fridge.thermo = c->fridge.thermo;
    s.fridge.thermo_cal = fridge_thermostat_calibrated(p);
    s.fridge.drive_known = p->drive_fridge != DRIVE_UNKNOWN;
    s.fridge.raw_v = ai(c, p->ai_fridge);
    s.fridge.temp_ts = c->fridge_temp_ts;
    s.fridge.diag_decoding = p->fridge_diag_decoder != DIAG_DECODER_NONE && p->fridge_diag_polarity != DIAG_POL_UNKNOWN;
    s.fridge.diag_transitions = c->fridge.diag.transitions;
    s.fridge.released = p->release_fridge;
    s.fridge.target_c = c->fridge.target_c;
    s.fridge.temp_c = c->fridge_temp_c;
    s.fridge.temp_q = c->fridge_q;
    s.fridge.fault_code = c->fridge.diag.code;
    s.heater.on = c->heater.on;
    s.heater.level = c->heater.level;
    s.heater.levels = p->heater_levels;
    s.heater.level_cal = p->heater_bits_cal;
    s.heater.phase = c->heater.phase;
    s.heater.released = p->release_heater;
    s.tanks.fresh_step = c->tanks.fresh_step;
    s.tanks.fresh_q = c->tanks.fresh_q;
    s.tanks.waste_full = c->tanks.waste_full;
    s.tanks.waste_q = c->tanks.waste_q;
    s.outside_c = c->outside_c;
    s.outside_q = c->outside_q;
    s.vehicle.ignition = control_di(c, p->di_ignition);
    s.vehicle.key86s = control_di(c, p->di_key86s);
    s.vehicle.dplus = control_di(c, p->di_dplus);
    s.vehicle.dim58 = control_di(c, p->di_dim58);
    s.vehicle.moving = c->vehicle.moving;
    s.vehicle.di_ok = c->safety.di_ok;
    s.vehicle.out_ok = c->safety.out_ok;
    s.energy.connected = c->energy.connected;
    for (int k = 0; k < EN_COUNT; k++) {
        s.energy.q[k] = energy_get(&c->energy, (energy_key_t)k, now, &s.energy.v[k]);
    }
    s.io.di = c->di;
    for (int i = 0; i < CU_N_RELAYS; i++) {
        s.io.relays |= (uint8_t)((c->written.relay[i] ? 1u : 0u) << i);
    }
    s.io.ladder = c->written.ladder;
    memcpy(s.io.ai, c->ai, sizeof s.io.ai);

    uint32_t a = 0;
    a |= c->safety.di_ok ? 0 : ALERT_DI_CARD;
    a |= c->safety.out_ok ? 0 : ALERT_OUT_CARD;
    a |= (c->vehicle.moving && c->roof.pos != ROOF_POS_CLOSED && p->roof_table_cal) ? ALERT_ROOF_OPEN_DRIVING : 0;
    a |= c->fridge.thermo == FRIDGE_SENSOR_FAULT ? ALERT_FRIDGE_SENSOR : 0;
    a |= c->fridge.diag.code ? ALERT_FRIDGE_FAULT : 0;
    a |= c->heater.phase == HEAT_FAULT ? ALERT_HEATER_FAULT : 0;
    a |= c->tanks.waste_full ? ALERT_WASTE_FULL : 0;
    a |= c->undervoltage ? ALERT_UNDERVOLTAGE : 0;
    a |= c->safety.conflicts ? ALERT_OUTPUT_CONFLICT : 0;
    s.alerts = a;
    state_bus_publish(c->bus, &s);
}

void control_logic_step(control_t *c, cu_ms_t now)
{
    lock(c);
    cu_cmd_t cmd;
    while (state_bus_pop(c->bus, &cmd)) {
        apply(c, &cmd, now);
    }
    fridge_step(&c->fridge, now, c->fridge_q, c->fridge_temp_c);
    undervoltage(c, now);
    publish(c, now);
    unlock(c);
}
