/* Release per function (profile release.*): false = measurement pending, never switches. */
#include "support.h"
#include "unity.h"

static rig_t r;

void setUp(void)
{
    rig_init(&r, NULL); /* the real profile: everything still TODO_MEASURE, nothing released */
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
}
void tearDown(void) {}

static void test_nothing_switches(void)
{
    rig_roof_hold(&r, ROOF_OPEN, false, 1000);
    cu_cmd_t c = {.type = CMD_HEATER, .on = true, .value = 3};
    state_bus_push(&r.bus, &c);
    c = (cu_cmd_t){.type = CMD_FRIDGE, .on = true, .value = 5};
    state_bus_push(&r.bus, &c);
    rig_run(&r, 400000);
    for (uint8_t ch = 1; ch <= 8; ch++) {
        TEST_ASSERT_FALSE(rig_relay(&r, ch));
    }
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.ladder);
    TEST_ASSERT_EQUAL(ROOF_BLOCK_NOT_RELEASED, r.c.roof.block);
}

static void test_measure_lock(void)
{
    rig_init(&r, "{\"outputs\":{\"roof_a\":{\"meaning\":\"open\",\"drive\":\"relay_plus\"},\"roof_b\":{\"meaning\":\"close\",\"drive\":\"relay_plus\"}},"
                 "\"measure\":{\"outputs_locked\":true},\"release\":{\"roof\":true,\"fridge\":true,\"heater\":true}}");
    rig_set_di(&r, 10, true);
    rig_run(&r, 400);
    rig_roof_hold(&r, ROOF_OPEN, false, 600);
    TEST_ASSERT_FALSE(rig_relay(&r, 1));
    TEST_ASSERT_EQUAL(ROOF_BLOCK_LOCKED, r.c.roof.block);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_switches);
    RUN_TEST(test_measure_lock);
    return UNITY_END();
}
