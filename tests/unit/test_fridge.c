/* Fridge: thermostat, cooling request output and diagnostics stay separate; unconfirmed
 * profiles never drive hardware; data loss and restart are defined (handover 1.11.2026, P3/P4).
 * All values here are synthetic test data, not vehicle measurements. */
#include <math.h>

#include "support.h"
#include "unity.h"

/* Calibrated looking profile for logic tests (synthetic). */
#define CAL_ANALOG                                                                                                   \
    "\"analog\":{\"fridge_temp\":{\"pullup_ohm\":10000},\"ref_3v3\":{\"divider\":1.2}},"                             \
    "\"curves\":{\"g817\":[[-10,55300],[5,25400],[25,10000]]},"
#define THERMO                                                                                                       \
    "\"fridge\":{\"target_default_c\":5,\"target_min_c\":1,\"target_max_c\":10,\"on_above_c\":1.5,\"off_below_c\":1.0," \
    "\"diag_polarity\":\"TODO_MEASURE\",\"diag_decoder\":\"none\"},"

static const char *const CONFIRMED = "{" CAL_ANALOG THERMO
                                     "\"outputs\":{\"fridge\":{\"drive\":\"relay_plus\"}},"
                                     "\"release\":{\"roof\":false,\"fridge\":true,\"heater\":false}}";
static const char *const NO_DRIVE = "{" CAL_ANALOG THERMO "\"release\":{\"roof\":false,\"fridge\":true,\"heater\":false}}";

static rig_t r;

static float volts_for_ohm(float ohm)
{
    return 3.3f * ohm / (ohm + 10000.0f);
}

static void warm(void)
{
    r.sim.ai[4] = 2.75f;                    /* AI5: 3.3 V reference via divider 1.2 */
    r.sim.ai[0] = volts_for_ohm(10000.0f); /* G817 at 25 degrees C (synthetic curve) */
}

static void fridge_on(void)
{
    cu_cmd_t c = {.type = CMD_FRIDGE, .on = true, .value = 5};
    state_bus_push(&r.bus, &c);
}

void setUp(void) {}
void tearDown(void) {}

static void test_real_profile_is_uncalibrated_and_never_drives(void)
{
    rig_init(&r, NULL);
    warm();
    fridge_on();
    rig_run(&r, 400000);
    TEST_ASSERT_EQUAL(FRIDGE_UNCALIBRATED, r.bus.state.fridge.thermo);
    TEST_ASSERT_TRUE(isnan(r.bus.state.fridge.target_c));
    TEST_ASSERT_TRUE(isnan(r.bus.state.fridge.temp_c)); /* no curve: no temperature, raw value only */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.65f, r.bus.state.fridge.raw_v);
    TEST_ASSERT_FALSE(rig_relay(&r, 3));
}

static void test_request_without_confirmed_drive_stays_simulated(void)
{
    rig_init(&r, NO_DRIVE);
    warm();
    fridge_on();
    rig_run(&r, 200000);
    TEST_ASSERT_TRUE(r.bus.state.fridge.request);  /* thermostat wants cooling */
    TEST_ASSERT_FALSE(r.bus.state.fridge.output);  /* but the output stage is unknown */
    TEST_ASSERT_FALSE(r.bus.state.fridge.drive_known);
    TEST_ASSERT_FALSE(rig_relay(&r, 3));
}

static void test_confirmed_profile_drives_output(void)
{
    rig_init(&r, CONFIRMED);
    warm();
    fridge_on();
    rig_run(&r, 200000);
    TEST_ASSERT_EQUAL(FRIDGE_DEMAND, r.bus.state.fridge.thermo);
    TEST_ASSERT_TRUE(r.bus.state.fridge.output);
    TEST_ASSERT_TRUE(rig_relay(&r, 3));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, r.bus.state.fridge.temp_c);
}

static void test_data_loss_turns_request_off_after_min_run(void)
{
    rig_init(&r, CONFIRMED);
    warm();
    fridge_on();
    rig_run(&r, 200000);
    TEST_ASSERT_TRUE(rig_relay(&r, 3));
    r.sim.fail_ai = true; /* analog card lost: temperature stale */
    rig_run(&r, 1000);
    TEST_ASSERT_TRUE(isnan(r.bus.state.fridge.temp_c)); /* never the last value */
    TEST_ASSERT_EQUAL(FRIDGE_SENSOR_FAULT, r.bus.state.fridge.thermo);
    rig_run(&r, 120000);
    TEST_ASSERT_FALSE(rig_relay(&r, 3));
    TEST_ASSERT_TRUE(r.bus.state.alerts & ALERT_FRIDGE_SENSOR);
}

static void test_restart_starts_with_request_off_and_pause(void)
{
    rig_init(&r, CONFIRMED);
    warm();
    fridge_on();
    rig_run(&r, 200000);
    TEST_ASSERT_TRUE(rig_relay(&r, 3));
    control_init(&r.c, &r.p, &r.io, &r.bus, r.now); /* restart */
    TEST_ASSERT_FALSE(rig_relay(&r, 3));
    fridge_on(); /* the wish is not stored yet (settings in LittleFS, later milestone) */
    rig_run(&r, 170000);
    TEST_ASSERT_FALSE(rig_relay(&r, 3)); /* S6 pause after restart */
    rig_run(&r, 20000);
    TEST_ASSERT_TRUE(rig_relay(&r, 3));
}

static void test_diag_recorded_raw_without_decoder(void)
{
    rig_init(&r, CONFIRMED);
    for (int i = 0; i < 6; i++) { /* arbitrary pulse train, synthetic */
        rig_set_di(&r, 3, true);
        rig_run(&r, 260);
        rig_set_di(&r, 3, false);
        rig_run(&r, 260);
    }
    rig_set_di(&r, 3, true);
    rig_run(&r, 5000);
    TEST_ASSERT_GREATER_OR_EQUAL(12, r.bus.state.fridge.diag_transitions);
    TEST_ASSERT_FALSE(r.bus.state.fridge.diag_decoding);
    TEST_ASSERT_EQUAL_UINT8(0, r.bus.state.fridge.fault_code);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_profile_is_uncalibrated_and_never_drives);
    RUN_TEST(test_request_without_confirmed_drive_stays_simulated);
    RUN_TEST(test_confirmed_profile_drives_output);
    RUN_TEST(test_data_loss_turns_request_off_after_min_run);
    RUN_TEST(test_restart_starts_with_request_off_and_pause);
    RUN_TEST(test_diag_recorded_raw_without_decoder);
    return UNITY_END();
}
