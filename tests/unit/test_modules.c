#include <math.h>

#include "support.h"
#include "unity.h"

static profile_t p;

void setUp(void)
{
    load_profile(&p, OVERLAY_RELEASED);
}
void tearDown(void) {}

/* Feeds a Secop code: n pulses of width ms low, 250 ms high between, then 4 s pause. */
static cu_ms_t feed_code(fridge_diag_t *d, cu_ms_t t, int n, int width)
{
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < width; k += 20, t += 20) {
            fridge_diag_sample(d, &p, t, false); /* active low */
        }
        for (int k = 0; k < 250; k += 20, t += 20) {
            fridge_diag_sample(d, &p, t, true);
        }
    }
    for (int k = 0; k < 4000; k += 20, t += 20) {
        fridge_diag_sample(d, &p, t, true);
    }
    return t;
}

static void test_fridge_codes_1_to_6_after_two_bursts(void)
{
    for (int code = 1; code <= 6; code++) {
        fridge_t f;
        fridge_init(&f, &p, 0);
        cu_ms_t t = feed_code(&f.diag, 100, code, 260);
        TEST_ASSERT_EQUAL_UINT8(0, f.diag.code); /* one burst is not enough */
        t = feed_code(&f.diag, t, code, 260);
        TEST_ASSERT_EQUAL_UINT8(code, f.diag.code);
    }
}

static void test_fridge_glitches_ignored(void)
{
    fridge_t f;
    fridge_init(&f, &p, 0);
    cu_ms_t t = 100;
    for (int k = 0; k < 3; k++) {
        t = feed_code(&f.diag, t, 2, 40); /* 40 ms spikes: not code pulses */
    }
    TEST_ASSERT_EQUAL_UINT8(0, f.diag.code);
    t = feed_code(&f.diag, t, 3, 260);
    t = feed_code(&f.diag, t, 4, 260); /* different codes: not confirmed */
    TEST_ASSERT_EQUAL_UINT8(0, f.diag.code);
}

static void test_fridge_code_clears(void)
{
    fridge_t f;
    fridge_init(&f, &p, 0);
    cu_ms_t t = feed_code(&f.diag, 100, 2, 260);
    t = feed_code(&f.diag, t, 2, 260);
    TEST_ASSERT_EQUAL_UINT8(2, f.diag.code);
    for (int k = 0; k < 12000; k += 20, t += 20) {
        fridge_diag_sample(&f.diag, &p, t, true);
    }
    TEST_ASSERT_EQUAL_UINT8(0, f.diag.code);
}

static void test_heater_phases(void)
{
    heater_t h;
    bool s;
    uint8_t lad;
    heater_init(&h, &p, 0);
    heater_set(&h, true, 5);
    heater_step(&h, 100, false, true, &s, &lad);
    TEST_ASSERT_EQUAL(HEAT_START, h.phase);
    TEST_ASSERT_TRUE(s);
    heater_step(&h, 5000, true, true, &s, &lad);
    TEST_ASSERT_EQUAL(HEAT_HEATING, h.phase);
    heater_set(&h, false, 5);
    heater_step(&h, 6000, true, true, &s, &lad);
    TEST_ASSERT_EQUAL(HEAT_AFTERRUN, h.phase);
    TEST_ASSERT_FALSE(s);
    heater_step(&h, 200000, false, true, &s, &lad);
    TEST_ASSERT_EQUAL(HEAT_OFF, h.phase);
}

static void test_heater_start_timeout(void)
{
    heater_t h;
    bool s;
    uint8_t lad;
    heater_init(&h, &p, 0);
    heater_set(&h, true, 5);
    heater_step(&h, 100, false, true, &s, &lad);
    heater_step(&h, 120100, false, true, &s, &lad);
    TEST_ASSERT_EQUAL(HEAT_FAULT, h.phase);
    TEST_ASSERT_FALSE(s);
}

static void test_tanks_uncalibrated(void)
{
    profile_t raw;
    load_profile(&raw, NULL);
    tanks_t t;
    tanks_init(&t, &raw);
    tanks_update(&t, 1.0f, 1.0f, 3.3f);
    TEST_ASSERT_EQUAL(CU_Q_UNCALIBRATED, t.fresh_q);
    TEST_ASSERT_EQUAL_INT8(-1, t.fresh_step);
    TEST_ASSERT_FALSE(t.waste_full);
}

static void test_energy_stale(void)
{
    energy_t e;
    float v;
    energy_init(&e, 10000);
    e.connected = true;
    energy_set(&e, EN_SOC, 76, 1000);
    TEST_ASSERT_EQUAL(CU_Q_OK, energy_get(&e, EN_SOC, 5000, &v));
    TEST_ASSERT_EQUAL_FLOAT(76, v);
    TEST_ASSERT_EQUAL(CU_Q_STALE, energy_get(&e, EN_SOC, 12000, &v));
    TEST_ASSERT_TRUE(isnan(v));
}

static void test_state_bus_queue_and_version(void)
{
    state_bus_t b;
    state_bus_init(&b);
    cu_cmd_t c = {.type = CMD_FRIDGE, .on = true, .value = 4}, out;
    for (int i = 0; i < STATE_BUS_QUEUE - 1; i++) {
        TEST_ASSERT_TRUE(state_bus_push(&b, &c));
    }
    TEST_ASSERT_FALSE(state_bus_push(&b, &c)); /* full */
    TEST_ASSERT_TRUE(state_bus_pop(&b, &out));
    TEST_ASSERT_EQUAL(CMD_FRIDGE, out.type);
    cu_state_t s = {0}, snap;
    state_bus_publish(&b, &s);
    state_bus_publish(&b, &s);
    state_bus_snapshot(&b, &snap);
    TEST_ASSERT_EQUAL_UINT32(2, snap.version);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fridge_codes_1_to_6_after_two_bursts);
    RUN_TEST(test_fridge_glitches_ignored);
    RUN_TEST(test_fridge_code_clears);
    RUN_TEST(test_heater_phases);
    RUN_TEST(test_heater_start_timeout);
    RUN_TEST(test_tanks_uncalibrated);
    RUN_TEST(test_energy_stale);
    RUN_TEST(test_state_bus_queue_and_version);
    return UNITY_END();
}
