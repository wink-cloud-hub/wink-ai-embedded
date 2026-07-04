/**
 * @file pal_wasm_fault.c
 * @brief Wasm 仿真端 fault 快速失败子系统 —— 三合一：
 *          1) fault 锁存 + App callbacks 引用（P0-3 Phase C；原在 pal_osal_wasm.c）
 *          2) Host→C fault 注入入口 pal_wasm_host_fault（原在 pal_osal_wasm.c）
 *          3) 故障审计日志环形缓冲区（ADR-0009 Wave 2 Task 8；原在 pal_wasm_physical.c）
 *
 * 三者强内聚：WASM_FAULT_GUARD_* 宏读锁存 → HAL 中间件在退化分支调用 log_fault →
 * JS 宿主抛错通过 host_fault 走 wink_runtime_fault 路径。放在同一 TU 便于阅读、
 * 减少跨 TU 依赖面（除 scheduler 与 reset_physical 的显式 helper 接口外，
 * 其它 pal_wasm_* 文件无需直连本文件的静态状态）。
 *
 * Scheduler 支持接口（本文件新增，仅供 pal_osal_wasm.c 内部调用）：
 *   - pal_wasm_fault_set_callbacks(cb)  scheduler_run 入口注册 App callbacks；
 *   - pal_wasm_invoke_fault(code)       WCET 兜底路径调用：置锁存 + 派发 fault。
 *
 * 与 pal_irq_wasm.c 采用相同 R-4 外层门控（`#if defined(__EMSCRIPTEN__)`），
 * 非 emcc target 编译单元为空，零污染。
 */
#include "pal_wasm_internal.h"
#include "wink_trace.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* wink_runtime_fault 的完整原型由 wink_runtime 提供，但为了避免把 runtime 头
 * 一路拖进本 target 内部文件（同时保持与原 pal_osal_wasm.c 一致的最小可见性），
 * 这里前向声明 struct wink_app_callbacks 并 extern 声明入口符号。 */
struct wink_app_callbacks;
extern void wink_runtime_fault(const struct wink_app_callbacks* callbacks, uint32_t fault_code);

/* ─────────────────────────────────────────────────────────
 * Fault 锁存（P0-3 Phase C）+ App callbacks 引用
 *
 * s_wasm_faulted：一旦宿主（JS 侧 plugin 抛错）或 WASM 内部（WCET、时钟溢出等）
 *                触发 fault，置 true 并保持；所有 pal_wasm_* 导出入口 fast-fail。
 *                由 pal_wasm_host_fault() / pal_wasm_invoke_fault() 置位；
 *                pal_wasm_reset_physical()（经由 pal_wasm_clear_fault_latch）清零。
 * s_app_callbacks：pal_sim_scheduler_run 入口通过 pal_wasm_fault_set_callbacks 注册，
 *                供 pal_wasm_host_fault / pal_wasm_invoke_fault 走标准 wink_runtime_fault
 *                路径（trace + safe_off_all + on_fault）。调度器窗口外
 *                （bootstrap 期以及 scheduler_run 返回之后——clear_fault_latch
 *                会同时清空 callbacks 引用）为 NULL，此时 host_fault 仍 trace
 *                并触发 safe-off，但无法派发 on_fault 回调。
 * ───────────────────────────────────────────────────────── */
static bool s_wasm_faulted = false;
static const struct wink_app_callbacks* s_app_callbacks = NULL;

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_is_faulted(void) { return s_wasm_faulted; }

void pal_wasm_clear_fault_latch(void) { s_wasm_faulted = false; s_app_callbacks = NULL; }

/* Scheduler 支持接口：由 pal_osal_wasm.c 的 pal_sim_scheduler_run 在进入 run
 * 周期时调用注册 App callbacks；本函数不改变 fault 锁存。 */
void pal_wasm_fault_set_callbacks(const struct wink_app_callbacks* cb) {
    s_app_callbacks = cb;
}

/* WCET 兜底路径（红线 16）等内部 C 侧 fault 注入：置锁存并派发 fault。
 * 幂等：与 pal_wasm_host_fault 语义一致——首次置位并走 wink_runtime_fault 全路径，
 * 后续调用仅 trace 不重复触发 safe-off。 */
void pal_wasm_invoke_fault(uint32_t code) {
    if (s_wasm_faulted) {
        wink_trace_fault(code);
        return;
    }
    s_wasm_faulted = true;
    wink_runtime_fault(s_app_callbacks, code);
}

/* ─────────────────────────────────────────────────────────
 * Host→C fault 注入（P0-3 Phase C）
 *
 * JS 侧宿主 plugin（用户自定义 js_* override）抛同步异常或返回 rejected Promise
 * 时，由 createUnisimImports 的 safeWrap/safeWrapAsync HOF 统一捕获，通过
 * _malloc + stringToUTF8 把错误消息写入线性内存，然后调用本函数走标准 fault
 * 路径（wink_trace_fault + wink_actuator_safe_off_all + on_fault 回调）。
 *
 * 调用约束：
 *   1. msg_cstr 必须是 wasm 线性内存内以 NUL 结尾的 UTF-8 C 字符串（由 JS 侧
 *      stringToUTF8OnStack / _malloc + stringToUTF8 写入）；调用方负责 _free。
 *   2. 本函数是幂等的——首次调用置位 s_wasm_faulted，后续调用仅 trace 不重复
 *      触发 safe-off（避免二次关断导致 actuator 状态异常）。
 *   3. 调度器窗口外（bootstrap 期或 scheduler_run 返回后 s_app_callbacks == NULL）
 *      safe-off 仍执行，但 on_fault 回调无法派发——理论上仅 init 期 JS 配置错误
 *      或 scheduler 生命周期外的 JS 主动 fault 注入会触发该路径。
 *
 * 错误码约定（review P0-3）：
 *   8003 — JS host plugin fault（用户 override 抛异常 / reject）
 * ───────────────────────────────────────────────────────── */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_host_fault(uint32_t code, const char* msg_cstr) {
    (void)msg_cstr;  /* TODO: Phase C — 把 msg 写入 fault ring buffer 供 UI 读取 */
    if (s_wasm_faulted) {
        /* 幂等：fault 锁存后只 trace 不再重复 safe-off */
        wink_trace_fault(code);
        return;
    }
    s_wasm_faulted = true;
    wink_runtime_fault(s_app_callbacks, code);
}

/* ─────────────────────────────────────────────────────────
 * 故障审计日志（ADR-0009 Wave 2 Task 8）
 * ─────────────────────────────────────────────────────────
 * Ring buffer of degradation events for CI causal-chain replay. All state
 * is BSS-zero on startup → no constructor. See pal_wasm_internal.h for
 * field semantics. The 4 KB cost (256 × 16B) is amortised at startup; the
 * design alternative (dynamic alloc) was rejected per §3.2 zero-dynamic-mem.
 *
 * Concurrency: wasm is single-threaded under Asyncify so no lock is needed.
 * If this ever migrates to wasm-threads, gate writes on a guard mutex.
 * ───────────────────────────────────────────────────────── */
static wasm_fault_event_t s_fault_log[WASM_FAULT_LOG_SIZE];
static uint32_t s_fault_log_head;    /* next write slot (mod WASM_FAULT_LOG_SIZE) */
static uint32_t s_fault_log_count;   /* total recorded, clamped at WASM_FAULT_LOG_SIZE */
static uint32_t s_fault_sequence;    /* global monotonically increasing seq, never wraps in practice */

/* ─────────────────────────────────────────────────────────
 * Fault audit log implementation
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
    WASM_FAULT_GUARD_VOID();
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

#endif /* __EMSCRIPTEN__ */
