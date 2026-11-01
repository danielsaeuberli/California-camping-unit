/* S1: first action after every start: K1-K8 off, H1-H4 off, before anything else. */
#include "support.h"
#include "unity.h"

static rig_t r;

void setUp(void) {}
void tearDown(void) {}

static void test_all_outputs_off_before_anything_else(void)
{
    rig_init(&r, OVERLAY_RELEASED); /* hal_sim starts with every relay on */
    TEST_ASSERT_GREATER_OR_EQUAL(9, r.sim.n_log);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_UINT8(i + 1, r.sim.log[i].ch);
        TEST_ASSERT_EQUAL_UINT8(0, r.sim.log[i].value);
    }
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.log[8].ch); /* ladder */
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.log[8].value);
    for (uint8_t ch = 1; ch <= 8; ch++) {
        TEST_ASSERT_FALSE(rig_relay(&r, ch));
    }
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.ladder);
}

static void test_restart_with_relays_held_by_the_card(void)
{
    /* The SM-H-001 keeps its relays over a P4 reset (SPEC 4.2): a new start must clear them. */
    rig_init(&r, OVERLAY_RELEASED);
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
    rig_roof_hold(&r, ROOF_OPEN, false, 500);
    TEST_ASSERT_TRUE(rig_relay(&r, 1));
    hal_sim_t keep = r.sim;
    control_init(&r.c, &r.p, &r.io, &r.bus, r.now); /* "reset" */
    TEST_ASSERT_TRUE(keep.relay[0]);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
}

static void test_failed_boot_write_marks_output_card_failed(void)
{
    rig_t f;
    load_profile(&f.p, OVERLAY_RELEASED);
    hal_sim_init(&f.sim, &f.io);
    f.sim.fail_out = true;
    state_bus_init(&f.bus);
    TEST_ASSERT_EQUAL_INT(9, control_init(&f.c, &f.p, &f.io, &f.bus, 0));
    TEST_ASSERT_FALSE(f.c.safety.out_ok);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_outputs_off_before_anything_else);
    RUN_TEST(test_restart_with_relays_held_by_the_card);
    RUN_TEST(test_failed_boot_write_marks_output_card_failed);
    return UNITY_END();
}
