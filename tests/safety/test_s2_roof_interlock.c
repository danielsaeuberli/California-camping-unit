/* S2: K1 and K2 never together; reversing only after 300 ms with both off. */
#include "support.h"
#include "unity.h"

static rig_t r;

void setUp(void)
{
    rig_init(&r, OVERLAY_RELEASED);
    rig_set_di(&r, 10, true); /* ignition */
    rig_run(&r, 400);
}
void tearDown(void) {}

static void test_reverse_waits_for_gap(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 1000);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    cu_ms_t k1_off = -1, k2_on = -1;
    for (int i = 0; i < 20; i++) { /* now press close */
        rig_roof(&r, ROOF_CLOSE, false);
        for (int t = 0; t < 10; t++) {
            rig_run(&r, 10);
            if (k1_off < 0 && !rig_relay(&r, 1)) {
                k1_off = r.now;
            }
            if (k2_on < 0 && rig_relay(&r, 2)) {
                k2_on = r.now;
            }
        }
    }
    TEST_ASSERT_TRUE(k1_off > 0 && k2_on > 0);
    TEST_ASSERT_GREATER_OR_EQUAL(300, k2_on - k1_off);
    TEST_ASSERT_EQUAL_INT(0, r.sim.both_roof_on);
}

static void test_safety_filter_drops_both(void)
{
    cu_outputs_t o = {0};
    o.relay[0] = o.relay[1] = true;
    safety_filter(&r.c.safety, &r.p, &o);
    TEST_ASSERT_FALSE(o.relay[0]);
    TEST_ASSERT_FALSE(o.relay[1]);
    TEST_ASSERT_EQUAL_UINT32(1, r.c.safety.conflicts);
}

static void test_restart_after_stop_waits_for_gap(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 500);
    rig_roof(&r, ROOF_STOP, false);
    rig_run(&r, 100);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    rig_roof(&r, ROOF_OPEN, false);
    rig_run(&r, 100);
    TEST_ASSERT_FALSE(rig_relay(&r, 1)); /* still inside the 300 ms gap */
    rig_roof_hold(&r, ROOF_OPEN, false, 300);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reverse_waits_for_gap);
    RUN_TEST(test_safety_filter_drops_both);
    RUN_TEST(test_restart_after_stop_waits_for_gap);
    return UNITY_END();
}
