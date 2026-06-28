#include "unity.h"
#include "wink_sim_physical.h"

void setUp(void) {}
void tearDown(void) {}

void test_prng_is_deterministic_and_matches_golden(void) {
    uint32_t s1 = 1, s2 = 1;
    /* golden: seed=1 → 1103527590，返回 ≈0.5138 */
    float r1 = wink_phys_prng_next(&s1);
    TEST_ASSERT_EQUAL_UINT32(1103527590u, s1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5138f, r1);
    /* 可复现：同种子同序列 */
    float r2 = wink_phys_prng_next(&s2);
    TEST_ASSERT_EQUAL_UINT32(s1, s2);
    TEST_ASSERT_EQUAL_FLOAT(r1, r2);
}

void test_prng_in_unit_range(void) {
    uint32_t s = 42;
    for (int i = 0; i < 1000; i++) {
        float r = wink_phys_prng_next(&s);
        TEST_ASSERT_TRUE(r >= 0.0f && r < 1.0f);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prng_is_deterministic_and_matches_golden);
    RUN_TEST(test_prng_in_unit_range);
    return UNITY_END();
}
