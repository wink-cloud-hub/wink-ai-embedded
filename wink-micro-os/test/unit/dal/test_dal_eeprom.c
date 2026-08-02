/*
 * test_dal_eeprom.c — host unit tests for dal_eeprom deinit hardening.
 *
 * The EEPROM driver is currently a stub (init returns WINK_ERR_UNSUPPORTED and
 * never sets initialized=true). These tests lock down the deinit contract so
 * that when the real I2C backend lands (Stage 4 oled_dashboard migration),
 * deinit already satisfies ADR-0024 §4 and does not regress.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_eeprom.h"
#include "pal_resource.h"

/* ADR-0017 层 1 例外：dal_eeprom_read_blocking/write_blocking are WINK_BLOCKING APIs;
 * host unit tests run outside the cooperative scheduler. */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

/* ---- deinit contract ---- */

void test_eeprom_deinit_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_eeprom_deinit(NULL));
}

void test_eeprom_deinit_uninitialized_is_idempotent_noop(void) {
    dal_eeprom_t dev = {0};
    /* deinit on a zeroed (uninitialized) instance must be a safe no-op. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_deinit(&dev));  /* double deinit still OK */
}

/* ---- init stub honesty ---- */

void test_eeprom_init_stub_reports_unsupported_and_does_not_claim(void) {
    dal_eeprom_t dev = {0};
    const dal_eeprom_config_t cfg = {
        .i2c_port = 0, .i2c_addr = 0x50, .capacity_bytes = 256,
        .page_size = 8, .write_time_ms = 5, .owner = "eeprom0",
    };
    /* Today the driver is an honest stub: must return UNSUPPORTED (not fake OK),
     * and must NOT claim the I2C address (no "假成功" 反模式, ADR-0012). */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    uint32_t res_id = pal_resource_i2c_id(0, 0x50);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));
}

void test_eeprom_rw_stub_returns_unsupported_and_leaves_buf_untouched(void) {
    dal_eeprom_t dev = {0};
    /* write on an uninitialized dev is rejected via early NULL/init checks
     * (in the real backend) or returns UNSUPPORTED for the stub. Pass NULL buf
     * to test the early invalid-arg path first, then use a real buffer. */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          dal_eeprom_write_blocking(&dev, 0, NULL, 3));
    const uint8_t w[3] = {'a', 'b', 'c'};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
                          dal_eeprom_write_blocking(&dev, 0, w, sizeof(w)));
    /* DAL-F-020: on error the out buffer MUST be left untouched (no 0xFF fill).
     * A real backend writes buf only on the WINK_OK path. */
    uint8_t out[4] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_read_blocking(&dev, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8(0x11, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x44, out[3]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eeprom_deinit_null_returns_invalid_arg);
    RUN_TEST(test_eeprom_deinit_uninitialized_is_idempotent_noop);
    RUN_TEST(test_eeprom_init_stub_reports_unsupported_and_does_not_claim);
    RUN_TEST(test_eeprom_rw_stub_returns_unsupported_and_leaves_buf_untouched);
    return UNITY_END();
}
