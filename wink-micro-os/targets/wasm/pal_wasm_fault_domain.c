/**
 * @file pal_wasm_fault_domain.c
 * @brief Wave3 前向兼容占位：故障域隔离框架 + 功耗模型 stub。
 *
 * ADR-0009 Wave 2 Tasks 9 & 10 落地的 ABI：把类型 + 符号 + JS 桥语义先冻结，
 * Wave3 在本文件内点亮真实逻辑时无需触碰任何调用点或 JS 桥（详见
 * pal_wasm_internal.h 内两个 header 段的说明）。
 *
 * 当前实现全部为 stub：
 *   - 故障域：所有合法域返回同一份全局 s_faults（由 pal_wasm_physical.c 暴露的
 *     pal_wasm_get_faults_ref() 提供，Wave3 会替换为 per-domain 数组）；
 *   - 功耗模型：set_pin_power_model 只做参数校验，不落存储；
 *     get_total_energy_mj 始终返回 0。
 *
 * 与 pal_irq_wasm.c 同 R-4 外层门控（`#if defined(__EMSCRIPTEN__)`）。
 */
#include "pal_wasm_internal.h"
#include "wink_status.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Fault domain state — BSS zero-init 后由 pal_wasm_reset_fault_domains 设为
 * 默认值（armed=true / trigger_count=0）。见头文件对首次访问契约的说明。 */
static wasm_fault_domain_t s_fault_domains[WASM_FAULT_DOMAIN_COUNT];

/* pal_wasm_reset_physical 内调用，初始化每域 id + armed=true。抽出成 helper
 * 是为了让 pal_wasm_physical.c 完全不 touch 本文件的静态状态。 */
void pal_wasm_reset_fault_domains(void) {
    /* Initialise per-domain state: id tag + default armed=true. We don't
     * rely on BSS zero here because "armed" must be true by default and
     * the id field must match the slot index. */
    for (uint32_t i = 0; i < WASM_FAULT_DOMAIN_COUNT; i++) {
        s_fault_domains[i].domain_id     = i;
        s_fault_domains[i].armed         = true;
        s_fault_domains[i].trigger_count = 0u;
    }
}

/* ─────────────────────────────────────────────────────────
 * 功耗模型 Stub 实现（Wave3 预埋；ADR-0009 Wave 2 Task 9）
 * ─────────────────────────────────────────────────────────
 * 当前实现为空占位，不做真实计算。提前锁定 ABI 是核心目的——Wave3 实施
 * 时无需碰任何调用点 / JS 桥即可在此函数体内点亮真实逻辑：
 *
 *   1. 在 BSS 增加 per-pin 模型存储（uint32 × 3 字段 × WASM_SIM_MAX_PINS
 *      ≈ 1.5 KB，与现有 s_debounce_ctx 同量级，符合 §3.2 零动态分配）。
 *   2. 在 GPIO/PWM 中间件（pal_hal_wasm.c）的电平翻转点累加跳变能量。
 *   3. 在 tick 边界按 P = I·V 公式对静态/有源电流积分。
 *   4. 把累计值（毫焦耳）通过 pal_wasm_get_total_energy_mj() 暴露给 JS。
 *
 * 与故障日志（Task 8）的关系：功耗事件不会被 pal_wasm_log_fault 记录——
 * 那个日志是"异常退化事件"通道，功耗是"持续物理量"通道，二者独立。
 *
 * 边界检查与其它 wasm 边界对称（pin >= WASM_SIM_MAX_PINS → 拒绝；
 * NULL 模型指针 → 拒绝）。stub 不解引用 model（仅校验非空），所以即便
 * 上层把垃圾指针传过来，本函数也不会越界读 wasm 堆。
 */
EMSCRIPTEN_KEEPALIVE
wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                           const wasm_pin_power_model_t *model) {
    WASM_FAULT_GUARD_WINKERR();
    if (pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (model == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    /* Stub: 不存储参数，仅验证接口可调用。Wave3 会在此处写入 BSS 数组。 */
    (void)pin;
    (void)model;
    return WINK_OK;
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_get_total_energy_mj(void) {
    /* Stub: 始终返回 0。Wave3 会在此处返回积分累计值。 */
    return 0;
}

/* ─────────────────────────────────────────────────────────
 * 故障域隔离框架实现（Wave3 预埋；ADR-0009 Wave 2 Task 10）
 * ─────────────────────────────────────────────────────────
 * 当前所有合法域都返回同一份全局 s_faults——这是设计意图，等效于"单域"
 * 但接口已就位。Wave3 实施清单（按此 file 内点亮，无需碰调用点）：
 *
 *   1. 把 s_fault_domains 的 wasm_fault_domain_t 扩展为含 wink_sim_faults_t
 *      实例（或改 wasm_fault_domain_t.config 为指针指向独立配置）。
 *   2. get_domain_config 改为返回 per-domain 配置。
 *   3. 修改 pal_hal_wasm.c 的 GPIO 抖动 / I2C 丢包注入点：从读全局
 *      pal_wasm_get_bounce_us() / pal_wasm_get_i2c_drop_permil() 改为按
 *      pin/port → domain_id 路由后调 pal_wasm_get_domain_config()->bounce_us。
 *   4. 在注入分支累加 s_fault_domains[domain].trigger_count，与
 *      pal_wasm_log_fault 形成"宏观计数 + 微观事件"双通道（与 Task 8 正交）。
 *
 * 越界处理：domain_id >= WASM_FAULT_DOMAIN_COUNT 时统一返回 sentinel
 * （NULL / INVALID_ARG / 0），杜绝 JS 数字越界写入 BSS。这与 power_model
 * stub 和 debounce_ctx 的越界契约对称（§3.3 plan）。
 *
 * 导出策略：当前仅供 C 侧测试和 Wave3 未来的 HAL 中间件调用，JS Worker
 * 不需要直接拨这些符号——Wave3 真要让 Workbench 控制单域时，再加一组
 * EMSCRIPTEN_KEEPALIVE 包装或在 wasm_bridge.h 暴露。
 */
wink_sim_faults_t *pal_wasm_get_domain_config(uint32_t domain_id) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return NULL;
    }
    /* TODO(Wave3): all domains alias the global s_faults instance held in
     * pal_wasm_physical.c. When Wave3 introduces per-domain config storage,
     * both this line and pal_wasm_get_faults_ref() go away together. */
    return pal_wasm_get_faults_ref();
}

wink_status_t pal_wasm_arm_fault_domain(uint32_t domain_id, bool armed) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }
    s_fault_domains[domain_id].armed = armed;
    return WINK_OK;
}

uint32_t pal_wasm_get_domain_trigger_count(uint32_t domain_id) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return 0u;
    }
    return s_fault_domains[domain_id].trigger_count;
}

#endif /* __EMSCRIPTEN__ */
