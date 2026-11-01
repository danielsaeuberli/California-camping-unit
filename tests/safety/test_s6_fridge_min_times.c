/* S6: fridge minimum run 2 min and minimum pause 3 min (profile). */
#include "support.h"
#include "unity.h"

static profile_t p;
static fridge_t f;

void setUp(void)
{
    load_profile(&p, OVERLAY_RELEASED);
    fridge_init(&f, &p, 0);
    fridge_set(&f, true, 5);
}
void tearDown(void) {}

static void test_pause_after_start(void)
{
    fridge_step(&f, 60000, CU_Q_OK, 10); /* warm, but within 3 min after start */
    TEST_ASSERT_FALSE(f.request);
    fridge_step(&f, 180000, CU_Q_OK, 10);
    TEST_ASSERT_TRUE(f.request);
}

static void test_minimum_run(void)
{
    fridge_step(&f, 180000, CU_Q_OK, 10);
    TEST_ASSERT_TRUE(f.request);
    fridge_step(&f, 200000, CU_Q_OK, 3); /* already cold enough after 20 s */
    TEST_ASSERT_TRUE(f.request);
    fridge_step(&f, 300000, CU_Q_OK, 3);
    TEST_ASSERT_FALSE(f.request);
}

static void test_minimum_pause(void)
{
    fridge_step(&f, 180000, CU_Q_OK, 10);
    fridge_step(&f, 300000, CU_Q_OK, 3);
    TEST_ASSERT_FALSE(f.request);
    fridge_step(&f, 400000, CU_Q_OK, 10);
    TEST_ASSERT_FALSE(f.request);
    fridge_step(&f, 480000, CU_Q_OK, 10);
    TEST_ASSERT_TRUE(f.request);
}

static void test_hysteresis(void)
{
    fridge_step(&f, 180000, CU_Q_OK, 6.4f); /* below target + 1.5 */
    TEST_ASSERT_FALSE(f.request);
    fridge_step(&f, 180100, CU_Q_OK, 6.5f);
    TEST_ASSERT_TRUE(f.request);
    fridge_step(&f, 400000, CU_Q_OK, 4.1f); /* above target - 1.0 */
    TEST_ASSERT_TRUE(f.request);
    fridge_step(&f, 400100, CU_Q_OK, 4.0f);
    TEST_ASSERT_FALSE(f.request);
}

static void test_sensor_fault_turns_off_after_min_run(void)
{
    fridge_step(&f, 180000, CU_Q_OK, 10);
    fridge_step(&f, 200000, CU_Q_OPEN, 0);
    TEST_ASSERT_EQUAL(FRIDGE_SENSOR_FAULT, f.thermo);
    TEST_ASSERT_TRUE(f.request); /* compressor protection first */
    fridge_step(&f, 300000, CU_Q_OPEN, 0);
    TEST_ASSERT_FALSE(f.request);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pause_after_start);
    RUN_TEST(test_minimum_run);
    RUN_TEST(test_minimum_pause);
    RUN_TEST(test_hysteresis);
    RUN_TEST(test_sensor_fault_turns_off_after_min_run);
    return UNITY_END();
}
