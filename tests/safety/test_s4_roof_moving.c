/* S4: opening blocked while the vehicle moves (edge counter DI6); closing allowed;
 * an opening run stops as soon as the vehicle starts moving. Roof also needs ignition. */
#include "support.h"
#include "unity.h"

static rig_t r;

static void drive(cu_ms_t ms, uint32_t edges_per_100ms)
{
    for (cu_ms_t t = 0; t < ms; t += 100) {
        r.sim.edges[5] += edges_per_100ms;
        rig_run(&r, 100);
    }
}

void setUp(void)
{
    rig_init(&r, OVERLAY_RELEASED);
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
}
void tearDown(void) {}

static void test_open_blocked_while_moving(void)
{
    drive(1000, 2); /* 20 edges/s >= 3 */
    TEST_ASSERT_TRUE(r.c.vehicle.moving);
    rig_roof(&r, ROOF_OPEN, false);
    drive(100, 2);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    TEST_ASSERT_EQUAL(ROOF_BLOCK_MOVING, r.c.roof.block);
}

static void test_close_allowed_while_moving(void)
{
    drive(1000, 2);
    for (int i = 0; i < 6; i++) {
        rig_roof(&r, ROOF_CLOSE, false);
        drive(100, 2);
    }
    TEST_ASSERT_TRUE(rig_relay(&r, 2));
}

static void test_opening_stops_when_driving_starts(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 600);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    for (int i = 0; i < 12; i++) {
        rig_roof(&r, ROOF_OPEN, false);
        drive(100, 2);
    }
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

static void test_few_edges_do_not_block(void)
{
    drive(1000, 0);
    r.sim.edges[5] += 1; /* single glitch, below moving_min_edges */
    rig_roof_hold(&r, ROOF_OPEN, false, 600);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
}

static void test_uncalibrated_threshold_blocks_on_any_edge(void)
{
    rig_init(&r, "{\"outputs\":{\"roof_a\":{\"meaning\":\"open\",\"drive\":\"relay_plus\"},\"roof_b\":{\"meaning\":\"close\",\"drive\":\"relay_plus\"}},"
                 "\"release\":{\"roof\":true,\"fridge\":false,\"heater\":false}}");
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
    r.sim.edges[5] += 1;
    rig_roof_hold(&r, ROOF_OPEN, false, 600);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

static void test_requires_ignition(void)
{
    rig_set_di(&r, 10, false);
    rig_roof_hold(&r, ROOF_CLOSE, false, 600);
    TEST_ASSERT_FALSE(rig_relay(&r, 2));
    TEST_ASSERT_EQUAL(ROOF_BLOCK_IGNITION, r.c.roof.block);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_blocked_while_moving);
    RUN_TEST(test_close_allowed_while_moving);
    RUN_TEST(test_opening_stops_when_driving_starts);
    RUN_TEST(test_few_edges_do_not_block);
    RUN_TEST(test_uncalibrated_threshold_blocks_on_any_edge);
    RUN_TEST(test_requires_ignition);
    return UNITY_END();
}
