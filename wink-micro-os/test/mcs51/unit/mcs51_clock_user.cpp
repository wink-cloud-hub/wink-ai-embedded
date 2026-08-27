// SPDX-License-Identifier: Apache-2.0
// M2 test user TU: a hand-written cleaned-style program (no Keil dialect, so no
// cleanup pass needed) that sleeps a virtual 100 ms then idles. Used by
// test_unisim_clock_mapping to verify the 1:1 master/slave clock mapping.
#include <stdint.h>

#include "wink_mcs51_clock.h"
#include "pal_osal.h"

// Observability captured right after the virtual sleep (defined here, read by
// the driver TU).
extern "C" {
uint64_t g_after_delay_virt_us = 0;
uint64_t g_after_delay_host_us = 0;
volatile uint8_t g_user_done = 0;
}

extern "C" void wink_mcs51_user_main(void) {
    // 100 ms virtual sleep == 10 master ticks (10 ms each), 1:1 (AD-14).
    wink_mcs51_delay_ms(100u);
    g_after_delay_virt_us = wink_mcs51_virtual_us();
    g_after_delay_host_us = pal_os_get_us();
    g_user_done = 1u;

    // Tight idle loop with an interception point: catch-up must keep the
    // slave clock conserved with master ticks without freezing (D3).
    for (;;) {
        wink_mcs51_microstep();
    }
}
