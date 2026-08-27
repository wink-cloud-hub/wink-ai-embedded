// SPDX-License-Identifier: Apache-2.0
// M2 timing test (clock SSOT §6.2 / ADR-0072 D1~D3): the 51 virtual slave
// clock maps 1:1 to master physical time.
//
//   * A 100 ms virtual delay (wink_mcs51_delay_ms) inside the fiber advances
//     the platform master clock by exactly 100 ms == 10 master ticks (10 ms).
//   * Virtual and master clocks agree at the wake point (no time inflation or
//     compression — the thermal integration scale the iron_ntc loop needs).
//   * Subsequent tight idle looping with interception points keeps advancing
//     via catch-up without freezing (quota yields occur, clocks stay 1:1).
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "pal_osal.h"

#include "wink_mcs51_clock.h"

extern "C" uint64_t g_after_delay_virt_us;
extern "C" uint64_t g_after_delay_host_us;
extern "C" volatile uint8_t g_user_done;

extern "C" const wink_app_callbacks_t* wink_app_get_callbacks(void);

#define RUN_TICKS 200u  // 200 * 10 ms = 2000 ms budget

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t* cb = wink_app_get_callbacks();
    if (cb == nullptr || cb->loop == nullptr) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    if (g_user_done == 0u) {
        printf("[mcs51] FAIL: fiber never reached post-delay point (frozen?)\n");
        fails++;
    }

    // 100 ms virtual sleep: slave clock at ~100,000 us after the sleep.
    if (g_after_delay_virt_us < 100000u || g_after_delay_virt_us > 101000u) {
        printf("[mcs51] FAIL: virtual clock after 100 ms delay = %llu us, "
               "want ~100000\n", (unsigned long long)g_after_delay_virt_us);
        fails++;
    }

    // 1:1 mapping: master (platform) clock advanced by the same 100 ms ==
    // 10 master ticks. Allow one-tick scheduling slack.
    if (g_after_delay_host_us < 90000u || g_after_delay_host_us > 110000u) {
        printf("[mcs51] FAIL: master clock after 100 ms delay = %llu us, "
               "want ~100000 (1:1 mapping broken)\n",
               (unsigned long long)g_after_delay_host_us);
        fails++;
    }

    // After the full bounded run, slave clock kept advancing via catch-up and
    // quota yields occurred (idle loop did not freeze).
    uint64_t virt_end = wink_mcs51_virtual_us();
    uint32_t yields   = wink_mcs51_quota_yield_count();
    if (virt_end < (uint64_t)RUN_TICKS * 10000u - 10000u) {
        printf("[mcs51] FAIL: end virtual clock %llu us < run budget "
               "(catch-up conservation broken)\n",
               (unsigned long long)virt_end);
        fails++;
    }
    if (yields == 0u) {
        printf("[mcs51] FAIL: no quota yields during idle loop\n");
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: 100 ms delay = virt %llu us / master %llu us "
           "(10 ticks, 1:1); end virt=%llu us, quota yields=%u\n",
           (unsigned long long)g_after_delay_virt_us,
           (unsigned long long)g_after_delay_host_us,
           (unsigned long long)virt_end, (unsigned)yields);
    return 0;
}
