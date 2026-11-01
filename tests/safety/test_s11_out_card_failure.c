/* S11: SM-H-001 fails (3 I2C errors in a row): roof blocked, alert, keep trying to switch off. */
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

static void test_blocked_and_retries_off(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 600);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    r.sim.fail_out = true;
    rig_roof_hold(&r, ROOF_OPEN, false, 400); /* refresh every 100 ms, 3 failures */
    TEST_ASSERT_FALSE(r.c.safety.out_ok);
    TEST_ASSERT_TRUE(r.bus.state.alerts & ALERT_OUT_CARD);
    TEST_ASSERT_EQUAL(ROOF_BLOCK_OUTPUTS, r.c.roof.block);
    r.sim.fail_out = false; /* card answers again: the off command gets through */
    rig_run(&r, 100);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blocked_and_retries_off);
    return UNITY_END();
}
