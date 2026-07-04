/**
 * @file pal_wasm_physical.c
 * @brief ADR-0009 Wave 2 — WASM-side wrapper around the physical degradation
 *        algorithm library (`wink_sim_physical.c`).
 *
 * Responsibilities:
 *   1. Hold the per-WASM-instance degradation state (faults config, PRNG,
 *      per-pin debounce contexts) — all in BSS, zero dynamic memory.
 *   2. Expose setters/getters across the WASM↔JS bridge so a JS Worker can
 *      drive fault injection without touching C internals.
 *   3. Provide an in-bounds, per-pin debounce context accessor for
 *      pal_hal_wasm.c's GPIO middleware (Wave 2 Task 3).
 *
 * Architecture invariants:
 *   - Zero dynamic memory (§3.2 of the plan). Static arrays only.
 *   - BSS-only initialisation: C standard guarantees zero-init for
 *     `static` storage; we do not memset at startup.
 *   - WASM_SIM_MAX_PINS = 128. Any pin index ≥ 128 returns NULL from the ctx
 *     getter and the caller (HAL middleware) must treat it as "no degradation
 *     for this pin". This prevents JS-supplied out-of-range indices from
 *     causing an OOB write into BSS.
 *   - PRNG state is global by design (see comment block below) — this is the
 *     ADR-0009 §4.1 "single seed reproduces the whole system" contract.
 *
 * Symbol export style:
 *   - `EMSCRIPTEN_KEEPALIVE` (matches pal_osal_wasm.c). It keeps the symbol
 *     past `-Oz` stripping and surfaces it on the export table when the
 *     linker is given `EXPORTED_FUNCTIONS=['_pal_wasm_*']` patterns.
 *   - Internal helpers (used by pal_hal_wasm.c only) have no KEEPALIVE.
 */
#include "wink_sim_physical.h"
#include "pal_wasm_internal.h"
#include "wasm_bridge.h"
#include "pal_hal.h"
#include "wink_status.h"

#include <emscripten.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────
 * Capacity
 * ─────────────────────────────────────────────────────────
 * 128 pins covers every embedded-class chip we ship to (ESP32-S3 has 49,
 * Cortex-M class boards <100). Each ctx is ~24B → ~3KB total: cheap.
 *
 * WASM_SIM_MAX_PINS is now defined in pal_wasm_internal.h so that
 * pal_hal_wasm.c can use it for its own pre-check (Task 3 §3.3 plan).
 */

/* ─────────────────────────────────────────────────────────
 * Global state (BSS — zero-initialised by the C runtime)
 * ─────────────────────────────────────────────────────────
 * No constructor / startup memset needed: spec [C11 §6.7.9 p10] says objects
 * with static storage duration and no explicit initialiser are zero-init.
 * Emscripten honours this; the .bss segment is zeroed by the loader before
 * main() runs.
 */

/* Fault config — initial all-zero == WINK_SIM_FAULTS_IDEAL == degradation off */
static wink_sim_faults_t s_faults;

/* Per-pin debounce contexts. Indexed by pin number ∈ [0, WASM_SIM_MAX_PINS). */
static wink_phys_debounce_ctx_t s_debounce_ctx[WASM_SIM_MAX_PINS];

/* Deterministic PRNG state.
 *
 * Design note (architectural intent, NOT a bug):
 *   Every degradation primitive that consumes randomness (I2C drop, RC noise,
 *   …) shares this single PRNG. This is deliberate: it makes "one seed
 *   reproduces the entire simulation" possible. The trade-off is that
 *   changing the call frequency of one peripheral (say, polling ADC twice as
 *   often) shifts the random sequence consumed by all others. For typical
 *   bug-repro scenarios — same code path, same input sequence — this is
 *   exactly what we want.
 *
 *   If a future use case ("isolate I2C flakiness from ADC noise during a
 *   parameter sweep") needs per-peripheral PRNGs, evolve to a struct of
 *   sub-states; do NOT silently split: it would break golden vectors.
 *
 * Default seed is 1 so wasm starts in the same state as the host golden
 * vectors. JS sets a real seed via pal_wasm_set_prng_seed() at init.
 */
static uint32_t s_prng_state = 1u;

/* ─────────────────────────────────────────────────────────
 * Fault audit log (ADR-0009 Wave 2 Task 8)
 * ─────────────────────────────────────────────────────────
 * Ring buffer of degradation events for CI causal-chain replay. All state
 * is BSS-zero on startup → no constructor. See pal_wasm_internal.h for
 * field semantics. The 4 KB cost (256 × 16B) is amortised at startup; the
 * design alternative (dynamic alloc) was rejected per §3.2 zero-dynamic-mem.
 *
 * Concurrency: wasm is single-threaded under Asyncify so no lock is needed.
 * If this ever migrates to wasm-threads, gate writes on a guard mutex.
 */
static wasm_fault_event_t s_fault_log[WASM_FAULT_LOG_SIZE];
static uint32_t s_fault_log_head;    /* next write slot (mod WASM_FAULT_LOG_SIZE) */
static uint32_t s_fault_log_count;   /* total recorded, clamped at WASM_FAULT_LOG_SIZE */
static uint32_t s_fault_sequence;    /* global monotonically increasing seq, never wraps in practice */

/* ─────────────────────────────────────────────────────────
 * Fault domain isolation framework (ADR-0009 Wave 2 Task 10 — Wave3 forward compat)
 * ─────────────────────────────────────────────────────────
 * BSS-resident table of per-domain state (armed flag + trigger counter). Today
 * only the GLOBAL domain is consulted by middleware, but the symbols are
 * exposed *now* so Wave3 can split s_faults per-domain (per-bus / per-pin)
 * without churning the wasm/JS bridge or every HAL call site.
 *
 * Storage cost: WASM_FAULT_DOMAIN_COUNT (=6) × sizeof(wasm_fault_domain_t)
 * = 6 × 12B = 72B in BSS. Trivial; the bigger ABI commitment is the enum
 * stability (see wasm_fault_domain_id_t — never reorder, only append).
 *
 * Default state after first call to pal_wasm_reset_physical():
 *   - All domains armed=true so existing tests / golden vectors keep firing.
 *   - All trigger_count=0.
 *
 * Note: On fresh wasm instance load (BSS-zero), armed=false for all slots.
 * JS Worker MUST call pal_wasm_reset_physical() during INIT phase before
 * reading domain state.
 *
 * Concurrency: same single-threaded Asyncify assumption as the fault log;
 * no lock needed today.
 */
static wasm_fault_domain_t s_fault_domains[WASM_FAULT_DOMAIN_COUNT];

/* ─────────────────────────────────────────────────────────
 * Fault config setters — exported to JS Worker
 * ─────────────────────────────────────────────────────────
 * One setter per field. JSON deserialisation lives in WasmPhysicalBridge.ts;
 * see plan §3.2 (zero dynamic memory → no cJSON, no malloc).
 *
 * All scalar types ≤32 bits → JS `number` is lossless (per BigInt contract).
 */

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_bounce_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.bounce_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_warmup_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.warmup_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_sample_interval_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.sample_interval_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_adc_noise_v(float v) { WASM_FAULT_GUARD_VOID(); s_faults.adc_noise_v = v; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_rc_tau_s(float s) { WASM_FAULT_GUARD_VOID(); s_faults.rc_tau_s = s; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_i2c_drop_permil(uint16_t permil) { WASM_FAULT_GUARD_VOID(); s_faults.i2c_drop_permil = permil; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_seed(uint32_t seed) { WASM_FAULT_GUARD_VOID(); s_prng_state = seed; }

/* ─────────────────────────────────────────────────────────
 * Internal getters
 * ─────────────────────────────────────────────────────────
 * Consumed by pal_hal_wasm.c (Wave 2 Task 3) and by unit tests.
 *
 * `pal_wasm_get_prng_state` is also exported because tests / JS may want to
 * snapshot the PRNG for "scenario replay" workflows. JS MUST NOT write the
 * state directly other than via pal_wasm_set_prng_seed() — that would break
 * the single-seed-reproduces-all-degradation invariant.
 */
uint32_t pal_wasm_get_bounce_us(void)        { return s_faults.bounce_us; }
uint16_t pal_wasm_get_i2c_drop_permil(void)  { return s_faults.i2c_drop_permil; }

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_prng_state(void)       { return s_prng_state; }

/* HAL middleware writes back the PRNG state after consuming bytes. Internal —
 * not exported; JS goes through pal_wasm_set_prng_seed() if it needs to
 * reseed. */
void pal_wasm_advance_prng_state(uint32_t new_state) { s_prng_state = new_state; }

/* ─────────────────────────────────────────────────────────
 * Per-pin debounce context accessor
 * ─────────────────────────────────────────────────────────
 * Returns NULL for any pin ≥ WASM_SIM_MAX_PINS. The HAL middleware treats
 * NULL as "no debounce context for this pin → fall through to ideal level",
 * preserving the bounds-check-but-don't-crash contract from the plan §3.3.
 *
 * BSS guarantees s_debounce_ctx[pin] starts with all fields zero, which
 * matches the documented "fresh ctx" state (stable=false, in_bounce=false,
 * bounce_start_us=0, bounce_flip=false). No runtime memset needed.
 */
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return NULL;
    }
    return &s_debounce_ctx[pin];
}

/* ─────────────────────────────────────────────────────────
 * Reset (test-only utility)
 * ─────────────────────────────────────────────────────────
 * memset is acceptable here even though BSS init was free, because we're
 * resetting from an arbitrary mid-test state, not initialising. JS test
 * harnesses call this between scenarios.
 *
 * Resets:
 *   - faults to all-zero (== ideal == no degradation)
 *   - every per-pin debounce ctx to fresh state
 *   - PRNG seed to the default of 1 (matches BSS init)
 *   - fault audit log to empty (delegates to pal_wasm_reset_fault_log)
 *   - fault domain table to defaults (all armed, zero trigger counts) so
 *     Wave3 middleware can rely on a deterministic baseline across runs
 */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_physical(void) {
    memset(&s_faults, 0, sizeof(s_faults));
    memset(s_debounce_ctx, 0, sizeof(s_debounce_ctx));
    s_prng_state = 1u;
    pal_wasm_reset_fault_log();
    pal_wasm_clear_fault_latch();

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
 * Fault audit log implementation (ADR-0009 Wave 2 Task 8)
 * ─────────────────────────────────────────────────────────
 * The "from-oldest-to-newest" iteration order in pal_wasm_get_fault_event
 * matters: CI replay reads the log forward in causal order, even after the
 * ring has wrapped. The index math below maps a logical index ∈ [0, count)
 * to the physical slot, accounting for both pre-wrap (count < SIZE, head ==
 * count) and post-wrap (count == SIZE, head points to the oldest = next
 * eviction slot) states.
 *
 * Sequence numbers are independent of ring eviction: an event evicted from
 * the ring still leaves its sequence baseline visible in surviving entries,
 * so CI can detect "the ring overflowed since I last looked" by comparing
 * the lowest visible sequence to its previous high-water mark.
 */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_fault_log(void) {
    memset(s_fault_log, 0, sizeof(s_fault_log));
    s_fault_log_head = 0;
    s_fault_log_count = 0;
    s_fault_sequence = 0;
}

void pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus) {
    wasm_fault_event_t *evt = &s_fault_log[s_fault_log_head];

    evt->timestamp_us = pal_wasm_get_virtual_clock_us();
    evt->fault_type   = fault_type;
    evt->pin_or_bus   = pin_or_bus;
    evt->sequence     = ++s_fault_sequence;

    s_fault_log_head = (s_fault_log_head + 1u) % WASM_FAULT_LOG_SIZE;
    if (s_fault_log_count < WASM_FAULT_LOG_SIZE) {
        s_fault_log_count++;
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_fault_log_count(void) {
    return s_fault_log_count;
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_get_fault_event(uint32_t index, wasm_fault_event_t *out_event) {
    if (out_event == NULL || index >= s_fault_log_count) {
        return false;
    }
    /* Map logical (oldest-first) index → physical slot.
     * Pre-wrap: head == count, oldest is at slot 0.
     * Post-wrap: head points to the about-to-be-evicted (= oldest) slot. */
    uint32_t oldest = (s_fault_log_head + WASM_FAULT_LOG_SIZE - s_fault_log_count)
                       % WASM_FAULT_LOG_SIZE;
    uint32_t actual_idx = (oldest + index) % WASM_FAULT_LOG_SIZE;
    *out_event = s_fault_log[actual_idx];
    return true;
}

/* Field-level accessors for the JS Worker.
 *
 * Rationale: cwrap'ing struct returns across the wasm/JS boundary is
 * Emscripten-specific and brittle (alignment, padding, BigInt-vs-number
 * for the 64-bit timestamp). Exposing one accessor per field is a small,
 * stable ABI that the worker calls per cell. Performance is fine: CI
 * post-mortem reads are not on the hot path.
 *
 * On out-of-range index every getter returns a sentinel zero — never reads
 * uninitialised memory. The "is this slot valid" question is answered by
 * pal_wasm_get_fault_log_count(), which the worker calls once up front.
 */
EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_fault_event_get_timestamp(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.timestamp_us : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint8_t pal_wasm_fault_event_get_type(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.fault_type : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint16_t pal_wasm_fault_event_get_pin_or_bus(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.pin_or_bus : 0u;
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_fault_event_get_sequence(uint32_t index) {
    wasm_fault_event_t evt;
    return pal_wasm_get_fault_event(index, &evt) ? evt.sequence : 0u;
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
    /* 当前所有域返回全局配置——Wave3 切换为 per-domain 实例时只改这一行。 */
    return &s_faults;
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

/* ─────────────────────────────────────────────────────────
 * JS-facing simplified exports (bool return, no out-pointer)
 * ─────────────────────────────────────────────────────────
 * These wrap the PAL functions that use wink_status_t + out-pointer
 * so WasmPhysicalBridge can call them with a simple bool-returning
 * signature matching what TS expects. Internal C callers continue to
 * use the full wink_status_t API from pal_hal.h. */

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_gpio_read(uint16_t pin) {
    bool level = false;
    wink_status_t st = pal_gpio_read((wink_pin_t)pin, &level);
    return wink_status_is_error(st) ? false : level;
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_i2c_transfer(uint8_t port, uint16_t dev_addr,
                           const uint8_t *write_buf, uint32_t write_len,
                           uint8_t *read_buf, uint32_t read_len) {
    wink_status_t st = pal_i2c_transfer(port, dev_addr, write_buf, write_len,
                                        read_buf, read_len);
    return !wink_status_is_error(st);
}
