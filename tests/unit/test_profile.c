#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "support.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_real_profile_loads_uncalibrated(void)
{
    profile_t p;
    load_profile(&p, NULL);
    TEST_ASSERT_EQUAL_UINT8(1, p.k_roof_a);
    TEST_ASSERT_EQUAL_UINT8(4, p.k_heater);
    TEST_ASSERT_EQUAL_UINT8(10, p.di_ignition);
    TEST_ASSERT_EQUAL_UINT8(6, p.di_speed);
    TEST_ASSERT_EQUAL_UINT8(8, p.ai_battery);
    TEST_ASSERT_EQUAL(ROOF_MEANING_UNKNOWN, p.roof_a_meaning);
    TEST_ASSERT_FALSE(p.g817.cal);
    TEST_ASSERT_FALSE(p.roof_table_cal);
    TEST_ASSERT_FALSE(p.roof_pump_afterrun_ms.cal);
    TEST_ASSERT_FALSE(p.speed_moving_min_edges.cal);
    TEST_ASSERT_FALSE(p.heater_bits_cal);
    TEST_ASSERT_FALSE(p.release_roof || p.release_fridge || p.release_heater);
    TEST_ASSERT_EQUAL_UINT8(10, p.heater_levels);
    TEST_ASSERT_EQUAL_UINT32(300, p.roof_hold_timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(60000, p.roof_max_run_ms);
    TEST_ASSERT_EQUAL_UINT32(120000, p.fridge_min_run_ms);
    TEST_ASSERT_TRUE(p.roof_tap_to_run);
}

static void test_heater_level_ohm_source_values_only(void)
{
    profile_t p;
    load_profile(&p, NULL);
    TEST_ASSERT_TRUE(p.heater_level_ohm[0].cal);
    TEST_ASSERT_EQUAL_FLOAT(333, p.heater_level_ohm[0].v);
    TEST_ASSERT_FALSE(p.heater_level_ohm[1].cal);
    TEST_ASSERT_EQUAL_FLOAT(839, p.heater_level_ohm[4].v);
    TEST_ASSERT_EQUAL_FLOAT(1993, p.heater_level_ohm[9].v);
}

static void expect_error(const char *overlay, const char *needle)
{
    char *base = read_file(CU_PROFILE_DIR "/t6-california-a8l-2018.json");
    profile_t p;
    char err[160];
    TEST_ASSERT_FALSE(profile_parse_with_overlay(base, overlay, &p, err, sizeof err));
    free(base);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, needle), err);
}

static void test_unknown_key_is_an_error(void)
{
    expect_error("{\"roof\":{\"speed_kmh\":3}}", "unknown key .roof.speed_kmh");
    expect_error("{\"extra\":1}", "unknown key .extra");
}

static void test_bad_values(void)
{
    expect_error("{\"outputs\":{\"roof_a\":{\"channel\":\"K9\"}}}", "K1..K8");
    expect_error("{\"inputs\":{\"speed\":{\"channel\":\"DI17\"}}}", "DI1..DI16");
    expect_error("{\"curves\":{\"g817\":[[0,1],[1,2]]}}", "three");
    expect_error("{\"outputs\":{\"roof_b\":{\"channel\":\"K1\"}}}", "different relays");
    expect_error("{\"heater\":{\"level_bits\":[0,1]}}", "one entry per level");
}

static void test_overlay_calibrates(void)
{
    profile_t p;
    load_profile(&p, OVERLAY_RELEASED);
    TEST_ASSERT_TRUE(p.roof_table_cal);
    TEST_ASSERT_EQUAL(ROOF_MEANING_OPEN, p.roof_a_meaning);
    TEST_ASSERT_TRUE(p.heater_bits_cal);
    TEST_ASSERT_TRUE(p.release_roof);
}

static void test_sim_overlay_is_valid(void)
{
    char *ov = read_file(CU_PROFILE_DIR "/sim/sim-overrides.json");
    profile_t p;
    load_profile(&p, ov);
    free(ov);
    TEST_ASSERT_TRUE(p.g817.cal);
    TEST_ASSERT_TRUE(p.roof_pump_afterrun_ms.cal);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_profile_loads_uncalibrated);
    RUN_TEST(test_heater_level_ohm_source_values_only);
    RUN_TEST(test_unknown_key_is_an_error);
    RUN_TEST(test_bad_values);
    RUN_TEST(test_overlay_calibrates);
    RUN_TEST(test_sim_overlay_is_valid);
    return UNITY_END();
}
