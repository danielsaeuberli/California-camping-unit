/* MultiPlus control (SPEC 5.1): only Mode and Ac/In/1/CurrentLimit, only with
 * victron.control, rate limited, confirmed by read-back. Synthetic data. */
#include <string.h>

#include "support.h"
#include "unity.h"

static profile_t p;
static energy_t e;
static victron_ctl_t v;

static const char *const ENABLED =
    "{\"victron\":{\"control\":true,\"modes\":[1,3,4],\"current_limit_a\":{\"min\":3,\"max\":16},"
    "\"confirm_s\":5,\"min_interval_s\":2}}";

static void setup(const char *overlay)
{
    load_profile(&p, overlay);
    energy_init(&e, 10000);
    e.connected = true;
    victron_ctl_init(&v, &p);
}

void setUp(void)
{
    setup(ENABLED);
}
void tearDown(void) {}

static void test_real_profile_never_writes(void)
{
    setup(NULL);
    TEST_ASSERT_EQUAL(VC_DISABLED, victron_request_mode(&v, &e, 4, 1000));
    vc_write_t w;
    TEST_ASSERT_FALSE(victron_pop_write(&v, &w));
}

static void test_mode_write_and_confirm(void)
{
    TEST_ASSERT_EQUAL(VC_OK, victron_request_mode(&v, &e, 1, 1000));
    vc_write_t w;
    TEST_ASSERT_TRUE(victron_pop_write(&v, &w));
    TEST_ASSERT_EQUAL(VW_MODE, w.key);
    TEST_ASSERT_EQUAL_FLOAT(1, w.value);
    energy_set(&e, EN_VEBUS_MODE, 1, 1500);
    victron_ctl_tick(&v, &e, 1600);
    TEST_ASSERT_FALSE(v.pending[VW_MODE]);
    TEST_ASSERT_EQUAL_INT8(-1, v.unconfirmed);
}

static void test_unconfirmed_after_timeout(void)
{
    victron_request_mode(&v, &e, 4, 1000);
    energy_set(&e, EN_VEBUS_MODE, 3, 1500); /* front switch keeps it on */
    victron_ctl_tick(&v, &e, 7000);
    TEST_ASSERT_EQUAL_INT8(VW_MODE, v.unconfirmed);
}

static void test_bad_mode_and_rate_limit(void)
{
    TEST_ASSERT_EQUAL(VC_BAD_MODE, victron_request_mode(&v, &e, 2, 1000)); /* inverter only not listed */
    TEST_ASSERT_EQUAL(VC_OK, victron_request_mode(&v, &e, 3, 1000));
    TEST_ASSERT_EQUAL(VC_TOO_FAST, victron_request_mode(&v, &e, 4, 2000));
    TEST_ASSERT_EQUAL(VC_OK, victron_request_mode(&v, &e, 4, 3100));
}

static void test_offline_rejected_and_queue_dropped(void)
{
    victron_request_mode(&v, &e, 4, 1000);
    e.connected = false;
    victron_ctl_tick(&v, &e, 1100);
    vc_write_t w;
    TEST_ASSERT_FALSE(victron_pop_write(&v, &w)); /* no replay after reconnect */
    TEST_ASSERT_EQUAL(VC_OFFLINE, victron_request_mode(&v, &e, 3, 5000));
}

static void test_current_limit(void)
{
    TEST_ASSERT_EQUAL(VC_LIMIT_FIXED, victron_request_limit(&v, &e, 10, 1000)); /* adjustable unknown */
    energy_set(&e, EN_LIMIT_ADJUSTABLE, 1, 1000);
    TEST_ASSERT_EQUAL(VC_OUT_OF_RANGE, victron_request_limit(&v, &e, 20, 1000));
    TEST_ASSERT_EQUAL(VC_OK, victron_request_limit(&v, &e, 10.04f, 1000));
    vc_write_t w;
    victron_pop_write(&v, &w);
    TEST_ASSERT_EQUAL(VW_CURRENT_LIMIT, w.key);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, w.value);
}

static void test_limit_disabled_without_min(void)
{
    setup("{\"victron\":{\"control\":true}}"); /* min still TODO_MEASURE */
    energy_set(&e, EN_LIMIT_ADJUSTABLE, 1, 1000);
    TEST_ASSERT_EQUAL(VC_LIMIT_DISABLED, victron_request_limit(&v, &e, 10, 1000));
}

static void test_only_two_topics(void)
{
    char t[64];
    TEST_ASSERT_EQUAL_INT(0, victron_write_topic(t, sizeof t, "c0619ab1", "276", VW_MODE));
    TEST_ASSERT_EQUAL_STRING("W/c0619ab1/vebus/276/Mode", t);
    TEST_ASSERT_EQUAL_INT(0, victron_write_topic(t, sizeof t, "c0619ab1", "276", VW_CURRENT_LIMIT));
    TEST_ASSERT_EQUAL_STRING("W/c0619ab1/vebus/276/Ac/In/1/CurrentLimit", t);
    TEST_ASSERT_EQUAL_INT(-1, victron_write_topic(t, sizeof t, "c0619ab1", "276", VW_COUNT));
    TEST_ASSERT_EQUAL_INT(-1, victron_write_topic(t, sizeof t, "", "276", VW_MODE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_profile_never_writes);
    RUN_TEST(test_mode_write_and_confirm);
    RUN_TEST(test_unconfirmed_after_timeout);
    RUN_TEST(test_bad_mode_and_rate_limit);
    RUN_TEST(test_offline_rejected_and_queue_dropped);
    RUN_TEST(test_current_limit);
    RUN_TEST(test_limit_disabled_without_min);
    RUN_TEST(test_only_two_topics);
    return UNITY_END();
}
