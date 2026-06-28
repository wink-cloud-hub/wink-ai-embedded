#include "wink_sim_physical.h"

const wink_sim_faults_t WINK_SIM_FAULTS_IDEAL = {0};  /* 全 0 = 理想，无退化 */

float wink_phys_prng_next(uint32_t *seed) {
    if (seed == NULL) { return 0.0f; }
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffffu;
    return (float)*seed / 2147483647.0f; /* 字面量统一 f 后缀，避免中间运算提升为 double，保 host/wasm/esp32 确定性一致 */
}

/* warmup_check 占位实现在 Task 5；此处先加 stub 让 Task 2 编译通过 */
wink_status_t wink_phys_warmup_check(uint64_t now_us, uint64_t power_on_us,
                                     uint32_t warmup_us, uint32_t sample_interval_us,
                                     uint64_t *last_sample_us) {
    (void)now_us; (void)power_on_us; (void)warmup_us;
    (void)sample_interval_us; (void)last_sample_us;
    return WINK_OK;  /* Task 5 替换 */
}
