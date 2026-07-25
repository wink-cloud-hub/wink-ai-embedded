/*
 * test_dal_gps.c — host unit tests for dal_gps deinit hardening.
 *
 * The GPS driver is currently a stub (init returns WINK_ERR_UNSUPPORTED, never
 * sets initialized=true). These tests lock down the deinit contract so the
 * future UART/NMEA backend satisfies ADR-0024 §4 from day one.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_gps.h"
#include "pal_resource.h"
#include <string.h>

/* ADR-0017 层 1 例外：dal_gps_init is a WINK_BLOCKING API; tests run on host
 * outside the cooperative scheduler, so suppress -Wdeprecated-declarations. */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

/* ---- deinit contract ---- */

void test_gps_deinit_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_deinit(NULL));
}

void test_gps_deinit_uninitialized_is_idempotent_noop(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_deinit(&dev));  /* double deinit still OK */
}

/* ---- init stub honesty ---- */

void test_gps_init_stub_reports_unsupported_and_does_not_claim(void) {
    dal_gps_t dev = {0};
    const dal_gps_config_t cfg = {
        .uart_port = 1, .baudrate = 9600, .owner = "gps0",
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_UART_PORT, 1));
}

void test_gps_poll_stub_returns_unsupported(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_poll(&dev));
}

void test_gps_get_position_safety_zeroes_output(void) {
    dal_gps_t dev = {0};
    dal_gps_position_t pos;
    /* Set sentinel bytes so we can detect the zero-fill. */
    memset(&pos, 0x55, sizeof(pos));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_get_position(&dev, &pos));
    /* All numeric fields must be zeroed (stub safety-fill), not left as
     * uninitialized stack garbage for the caller. memset(0) on IEEE 754 floats
     * produces positive zero 0.0f; use a small epsilon for Unity compatibility. */
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, pos.latitude);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, pos.longitude);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, pos.altitude_m);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, pos.speed_kmh);
    TEST_ASSERT_EQUAL_UINT8(0, pos.satellites);
    TEST_ASSERT_FALSE(pos.fix_valid);
    TEST_ASSERT_EQUAL_UINT32(0, pos.timestamp_ms);
}

void test_gps_get_position_null_returns_invalid_arg(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_get_position(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_get_position(NULL, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gps_deinit_null_returns_invalid_arg);
    RUN_TEST(test_gps_deinit_uninitialized_is_idempotent_noop);
    RUN_TEST(test_gps_init_stub_reports_unsupported_and_does_not_claim);
    RUN_TEST(test_gps_poll_stub_returns_unsupported);
    RUN_TEST(test_gps_get_position_safety_zeroes_output);
    RUN_TEST(test_gps_get_position_null_returns_invalid_arg);
    return UNITY_END();
}
