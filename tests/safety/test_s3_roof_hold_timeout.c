/* S3: roof relay drops 300 ms after the last run message, and after max_run_s.
 * Applies to holding and to tapping (the UI confirms the tap run every 100 ms). */
#include "support.h"
#include "unity.h"

static rig_t r;

void setUp(void)
{
    rig_init(&r, OVERLAY_RELEASED);
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
}
void tearDown(void) {}

static void test_hold_released_stops_within_300ms(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 1000);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    cu_ms_t last = r.now; /* finger lifted, no more messages */
    while (rig_relay(&r, 1) && r.now - last < 1000) {
        rig_run(&r, 10);
    }
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    TEST_ASSERT_LESS_OR_EQUAL(300 + 100 + 20, r.now - last); /* message period + io cycle */
    TEST_ASSERT_EQUAL(ROOF_BLOCK_NO_CONFIRM, r.c.roof.block);
}

static void test_max_run_time(void)
{
    rig_roof_hold(&r, ROOF_CLOSE, false, 59000);
    TEST_ASSERT_TRUE(rig_relay(&r, 2));
    rig_roof_hold(&r, ROOF_CLOSE, false, 2000);
    TEST_ASSERT_FALSE(rig_relay(&r, 2));
    TEST_ASSERT_EQUAL(ROOF_BLOCK_MAX_RUN, r.c.roof.block);
}

static void test_tap_runs_to_end_position(void)
{
    TEST_ASSERT_TRUE(roof_tap_available(&r.p));
    rig_set_di(&r, 1, false);
    rig_set_di(&r, 2, true); /* closed */
    rig_roof_hold(&r, ROOF_OPEN, true, 2000);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    rig_set_di(&r, 1, true);
    rig_set_di(&r, 2, false); /* open reached */
    rig_roof_hold(&r, ROOF_OPEN, true, 300);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    TEST_ASSERT_EQUAL(ROOF_POS_OPEN, r.c.roof.pos);
}

static void test_tap_stops_when_confirmations_stop(void)
{
    rig_roof_hold(&r, ROOF_OPEN, true, 1000);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    rig_run(&r, 450); /* UI task hangs */
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

static void test_tap_without_truth_table_is_hold(void)
{
    rig_init(&r, "{\"outputs\":{\"roof_a\":{\"meaning\":\"open\",\"drive\":\"relay_plus\"},\"roof_b\":{\"meaning\":\"close\",\"drive\":\"relay_plus\"}},"
                 "\"release\":{\"roof\":true,\"fridge\":false,\"heater\":false}}");
    TEST_ASSERT_FALSE(roof_tap_available(&r.p));
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
    rig_roof_hold(&r, ROOF_OPEN, true, 500);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    TEST_ASSERT_FALSE(r.c.roof.auto_run);
}

static void test_old_messages_are_ignored(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 500);
    cu_cmd_t old = {.type = CMD_ROOF, .seq = 1, .dir = ROOF_OPEN};
    rig_run(&r, 400); /* stopped by S3 */
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    state_bus_push(&r.bus, &old); /* a stale message must not restart the roof */
    rig_run(&r, 400);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hold_released_stops_within_300ms);
    RUN_TEST(test_max_run_time);
    RUN_TEST(test_tap_runs_to_end_position);
    RUN_TEST(test_tap_stops_when_confirmations_stop);
    RUN_TEST(test_tap_without_truth_table_is_hold);
    RUN_TEST(test_old_messages_are_ignored);
    return UNITY_END();
}
