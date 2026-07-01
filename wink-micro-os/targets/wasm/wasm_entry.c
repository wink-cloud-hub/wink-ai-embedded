/**
 * @file wasm_entry.c
 * @brief Wasm 入口：main()。
 *        从旧 pal_hal_wasm.c 拆出；target entry 只负责启动 runtime（03-dir §7）。
 *
 * 中断桥变更（ADR-0002 方案 C）：
 *   旧 _trigger_wasm_interrupt 导出已移除。JS 侧改为将中断事件写入 pending 队列，
 *   由 Wasm C 侧在 tick 边界主动拉取（pal_wasm_dispatch_pending_interrupts），
 *   彻底消除 Asyncify sleeping 窗口重入面（D1 修复，见 docs/04 §4）。
 */
#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
#include "pal_hal.h"
#include "wink_app.h"
#include "wink_runtime.h"

/* App 工厂（由注入的 App 提供，wasm 构建链接 samples 或用户 App） */
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);


int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    /* 返回值遵循 wink_status_t 约定：Wasm 下 Asyncify 会持续让出 → 正常场景走不到此处；
     * 若 runtime 因契约错误提前退出，把它变成进程退出码方便 JS 侧诊断（负=错误）。 */
    return (int)wink_runtime_run(cb, 0);   /* 0 = 无限循环（wasm 下由 Asyncify 让出） */
}
