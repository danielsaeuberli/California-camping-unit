#include <math.h>

#include "signals.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_debounce(void)
{
    sig_debounce_t d;
    sig_debounce_init(&d, 2);
    TEST_ASSERT_FALSE(sig_debounce(&d, true));
    TEST_ASSERT_TRUE(sig_debounce(&d, true));
    TEST_ASSERT_TRUE(sig_debounce(&d, false));
    TEST_ASSERT_FALSE(sig_debounce(&d, false));
}

static void test_edges_in_window(void)
{
    sig_edges_t e;
    sig_edges_init(&e, 1000);
    uint32_t c = 0;
    for (int t = 0; t <= 2000; t += 20) {
        c += (t % 100 == 0) ? 1 : 0; /* 10 edges per second */
        sig_edges_update(&e, t, c);
    }
    TEST_ASSERT_UINT32_WITHIN(1, 10, sig_edges_update(&e, 2020, c));
    TEST_ASSERT_EQUAL_UINT32(0, sig_edges_update(&e, 3100, c));
}

static void test_ohm_from_volts(void)
{
    float ohm;
    cu_cal_t pu = {10000, true};
    TEST_ASSERT_EQUAL(CU_Q_OK, sig_ohm_from_volts(1.65f, 3.3f, pu, &ohm));
    TEST_ASSERT_FLOAT_WITHIN(1, 10000, ohm);
    TEST_ASSERT_EQUAL(CU_Q_OPEN, sig_ohm_from_volts(3.29f, 3.3f, pu, &ohm));
    TEST_ASSERT_EQUAL(CU_Q_SHORT, sig_ohm_from_volts(0.01f, 3.3f, pu, &ohm));
    TEST_ASSERT_EQUAL(CU_Q_UNCALIBRATED, sig_ohm_from_volts(1.0f, 3.3f, (cu_cal_t){0, false}, &ohm));
    TEST_ASSERT_TRUE(isnan(ohm));
}

static void test_ntc_three_points(void)
{
    profile_ntc_t n = {{-10, 5, 25}, {55300, 25400, 10000}, true};
    float t;
    TEST_ASSERT_EQUAL(CU_Q_OK, sig_ntc_temp(&n, 25400, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 5, t);
    TEST_ASSERT_EQUAL(CU_Q_OK, sig_ntc_temp(&n, 10000, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 25, t);
    n.cal = false;
    TEST_ASSERT_EQUAL(CU_Q_UNCALIBRATED, sig_ntc_temp(&n, 10000, &t));
}

static void test_median(void)
{
    sig_median_t m;
    sig_median_init(&m, 5);
    float v[] = {1, 100, 2, 3, -50};
    float r = 0;
    for (int i = 0; i < 5; i++) {
        r = sig_median_add(&m, v[i]);
    }
    TEST_ASSERT_EQUAL_FLOAT(2, r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_debounce);
    RUN_TEST(test_edges_in_window);
    RUN_TEST(test_ohm_from_volts);
    RUN_TEST(test_ntc_three_points);
    RUN_TEST(test_median);
    return UNITY_END();
}
