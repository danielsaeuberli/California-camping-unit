/* S5: SM-I-009 fails (3 I2C errors in a row): DI stale, roof blocked, alert. */
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

static void test_running_roof_stops_and_alert(void)
{
    rig_roof_hold(&r, ROOF_CLOSE, false, 600);
    TEST_ASSERT_TRUE(rig_relay(&r, 2));
    r.sim.fail_di = true;
    rig_roof_hold(&r, ROOF_CLOSE, false, 200);
    TEST_ASSERT_FALSE(rig_relay(&r, 2));
    TEST_ASSERT_FALSE(r.c.safety.di_ok);
    TEST_ASSERT_TRUE(r.bus.state.alerts & ALERT_DI_CARD);
}

static void test_two_errors_are_tolerated(void)
{
    safety_t s;
    safety_init(&s);
    safety_note_di(&s, false);
    safety_note_di(&s, false);
    TEST_ASSERT_TRUE(s.di_ok);
    safety_note_di(&s, true);
    safety_note_di(&s, false);
    safety_note_di(&s, false);
    TEST_ASSERT_TRUE(s.di_ok);
    safety_note_di(&s, false);
    TEST_ASSERT_FALSE(s.di_ok);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_running_roof_stops_and_alert);
    RUN_TEST(test_two_errors_are_tolerated);
    return UNITY_END();
}
