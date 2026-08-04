/*
 * test_pal_i2c_bus.c — host unit tests for pal_i2c_bus_init / pal_i2c_bus_deinit.
 *
 * Stage 0 Task 0.1: PAL 层极简 I2C bus 生命周期 API。单器件 DAL _init/_deinit
 * 继续按 (port, dev_addr) 直接调 pal_i2c_transfer；bus_init/bus_deinit 仅给
 * codegen 生成的静态 bus-owner 节点调用（ADR-0024 §4 #6）。
 *
 * 本测试验证 host 侧 bus_init/deinit 记录了 inited 状态，且 pal_i2c_transfer
 * 在未 init 时会 warn+lazily-init（Stage 0 过渡路径；Stage 1.6 codegen bus-owner
 * 生成到位后该 lazy 路径会被移除，测试届时会更新）。
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_resource.h"
#include "hal/pal_i2c.h"
#include "host_test_ctrl.h"

/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING 的 pal_i2c_transfer 来验证
 * bus_init→transfer 通路（pal_i2c_transfer 是阻塞 API）。抑制 -Wdeprecated-declarations
 * 使 -Werror 下仍能编译；严格模式下 pal_i2c_transfer 声明本身消失，本 TU 不会被编译。 */
#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) {
    pal_resource_reset();
    sim_reset_time();
}
void tearDown(void) {}

/* bus_init 对合法 port 返 OK，越界返 INVALID_ARG */
void test_bus_init_rejects_invalid_port(void) {
    /* PAL_I2C_PORTS 通常是 2；port 2 应被拒 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_i2c_bus_init(PAL_I2C_PORTS, 21, 22, 100000));
    /* 合法 port */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    /* 重复 init 应是幂等 no-op，返回 OK（而不是 BUSY）—— device_tree 幂等性
     * 或异常恢复路径需要这个语义 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    pal_i2c_bus_deinit(0);
}

/* bus_deinit 对未 init 的 port 无副作用（幂等） */
void test_bus_deinit_idempotent_on_uninited_port(void) {
    /* 先确保未 init */
    pal_i2c_bus_deinit(1);
    pal_i2c_bus_deinit(1);
    /* 越界 port 不崩溃 */
    pal_i2c_bus_deinit(PAL_I2C_PORTS);
    TEST_PASS();
}

/* 基本 init→transfer→deinit 流程可走通（host 侧 transfer 永远返 OK） */
void test_bus_init_enables_transfer(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    uint8_t w = 0x00;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
    pal_i2c_bus_deinit(0);
}

/* deinit 后 transfer 在 Stage 0 过渡阶段会 lazy-init 并打 WARN（验证 lazy 路径
 * 存在，Stage 1.6 收紧为返 INVALID_STATE 时更新本测试）。 */
void test_transfer_after_deinit_lazy_reinits_as_transitional_behavior(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    pal_i2c_bus_deinit(0);
    /* Stage 0 过渡：transfer 应不崩溃（warn + lazy inited） */
    uint8_t w = 0xAE;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    /* 清理：重新 deinit */
    pal_i2c_bus_deinit(0);
}

/* 多 port 独立：port0 deinit 不影响 port1 */
void test_bus_ports_are_independent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(1, 33, 34, 400000));
    /* 两个 port 都能发 transfer */
    uint8_t w = 0x00;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(1, 0x50, &w, 1, NULL, 0));
    /* 只 deinit port0 */
    pal_i2c_bus_deinit(0);
    /* port1 仍可用（Stage 0 下 lazy 路径保活，此处主要验证不 crash） */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(1, 0x50, &w, 1, NULL, 0));
    pal_i2c_bus_deinit(1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bus_init_rejects_invalid_port);
    RUN_TEST(test_bus_deinit_idempotent_on_uninited_port);
    RUN_TEST(test_bus_init_enables_transfer);
    RUN_TEST(test_transfer_after_deinit_lazy_reinits_as_transitional_behavior);
    RUN_TEST(test_bus_ports_are_independent);
    return UNITY_END();
}
