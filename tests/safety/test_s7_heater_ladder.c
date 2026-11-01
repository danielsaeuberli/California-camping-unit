/* S7: heater level relays H1-H4 switch one at a time, 50 ms apart. */
#include <stdlib.h>

#include "support.h"
#include "unity.h"

static rig_t r;

void setUp(void)
{
    rig_init(&r, OVERLAY_RELEASED);
    rig_run(&r, 100);
}
void tearDown(void) {}

static int popcount(unsigned v)
{
    int n = 0;
    for (; v; v &= v - 1) {
        n++;
    }
    return n;
}

static void test_one_relay_per_step(void)
{
    cu_cmd_t c = {.type = CMD_HEATER, .on = false, .value = 7}; /* bits 0111 */
    state_bus_push(&r.bus, &c);
    uint8_t prev = r.sim.ladder;
    cu_ms_t last_change = 0;
    for (int i = 0; i < 100; i++) {
        rig_run(&r, 10);
        if (r.sim.ladder != prev) {
            TEST_ASSERT_EQUAL_INT(1, popcount(r.sim.ladder ^ prev));
            if (last_change) {
                TEST_ASSERT_GREATER_OR_EQUAL(50, r.now - last_change);
            }
            last_change = r.now;
            prev = r.sim.ladder;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(7, r.sim.ladder);
    c.value = 8; /* 1000: four relays change */
    state_bus_push(&r.bus, &c);
    for (int i = 0; i < 100; i++) {
        rig_run(&r, 10);
        if (r.sim.ladder != prev) {
            TEST_ASSERT_EQUAL_INT(1, popcount(r.sim.ladder ^ prev));
            TEST_ASSERT_GREATER_OR_EQUAL(50, r.now - last_change);
            last_change = r.now;
            prev = r.sim.ladder;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(8, r.sim.ladder);
}

static void test_uncalibrated_ladder_stays_bridged(void)
{
    rig_init(&r, "{\"outputs\":{\"heater\":{\"drive\":\"relay_plus\"}},\"release\":{\"roof\":false,\"fridge\":false,\"heater\":true}}");
    cu_cmd_t c = {.type = CMD_HEATER, .on = true, .value = 7};
    state_bus_push(&r.bus, &c);
    rig_run(&r, 500);
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.ladder);
    TEST_ASSERT_EQUAL_UINT8(r.p.heater_level_default, r.c.heater.level);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_one_relay_per_step);
    RUN_TEST(test_uncalibrated_ladder_stays_bridged);
    return UNITY_END();
}
