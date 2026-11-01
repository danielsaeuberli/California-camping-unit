#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

const char *const OVERLAY_RELEASED =
    "{\"outputs\":{\"roof_a\":{\"meaning\":\"open\",\"drive\":\"relay_plus\"},"
    "\"roof_b\":{\"meaning\":\"close\",\"drive\":\"relay_plus\"},"
    "\"fridge\":{\"drive\":\"relay_plus\"},\"heater\":{\"drive\":\"relay_plus\"}},"
    "\"fridge\":{\"target_default_c\":5,\"target_min_c\":1,\"target_max_c\":10,\"on_above_c\":1.5,"
    "\"off_below_c\":1.0,\"diag_polarity\":\"low\",\"diag_decoder\":\"secop_hypothesis\"},"
    "\"roof\":{\"truth_table\":{\"closed\":[0,1],\"open\":[1,0],\"between\":[0,0]}},"
    "\"speed\":{\"moving_min_edges\":3},"
    "\"heater\":{\"level_bits\":[0,1,2,3,4,5,6,7,8,9]},"
    "\"release\":{\"roof\":true,\"fridge\":true,\"heater\":true}}";

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    TEST_ASSERT_EQUAL(n, (long)fread(buf, 1, (size_t)n, f));
    buf[n] = 0;
    fclose(f);
    return buf;
}

void load_profile(profile_t *p, const char *overlay)
{
    char *base = read_file(CU_PROFILE_DIR "/t6-california-a8l-2018.json");
    char err[160];
    bool ok = profile_parse_with_overlay(base, overlay, p, err, sizeof err);
    free(base);
    TEST_ASSERT_TRUE_MESSAGE(ok, err);
}

void rig_init(rig_t *r, const char *overlay)
{
    memset(r, 0, sizeof *r);
    load_profile(&r->p, overlay);
    hal_sim_init(&r->sim, &r->io);
    state_bus_init(&r->bus);
    r->now = 1000;
    control_init(&r->c, &r->p, &r->io, &r->bus, r->now);
}

void rig_run(rig_t *r, cu_ms_t ms)
{
    for (cu_ms_t t = 0; t < ms; t += 10) {
        r->now += 10;
        if (r->now % 20 == 0) {
            control_io_step(&r->c, r->now);
        }
        if (r->now % 50 == 0) {
            control_logic_step(&r->c, r->now);
        }
    }
}

void rig_roof(rig_t *r, roof_dir_t dir, bool auto_run)
{
    cu_cmd_t cmd = {.type = CMD_ROOF, .seq = ++r->seq, .dir = dir, .auto_run = auto_run};
    state_bus_push(&r->bus, &cmd);
}

void rig_roof_hold(rig_t *r, roof_dir_t dir, bool auto_run, cu_ms_t ms)
{
    for (cu_ms_t t = 0; t < ms; t += 100) {
        rig_roof(r, dir, auto_run);
        rig_run(r, 100);
    }
}

void rig_set_di(rig_t *r, uint8_t ch, bool on)
{
    if (on) {
        r->sim.di |= (uint16_t)(1u << (ch - 1));
    } else {
        r->sim.di &= (uint16_t)~(1u << (ch - 1));
    }
}

bool rig_relay(const rig_t *r, uint8_t ch)
{
    return r->sim.relay[ch - 1];
}
