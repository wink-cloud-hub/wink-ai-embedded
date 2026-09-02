// SPDX-License-Identifier: Apache-2.0
// Unit test for wink_mcu.h MCU facade routing.
#include <stdint.h>
#include <stdio.h>

#ifndef WINK_MCU_CMS8S78XX
#define WINK_MCU_CMS8S78XX 1
#endif

#include "wink_mcu.h"

// Undefine the Keil dialect main remap so the test runner main() can link
#ifdef main
#undef main
#endif

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    // 1. Verify that CMS8S78xx SFRs are defined and proxied correctly
    ADCCHS = 0x05;
    if (static_cast<uint8_t>(ADCCHS) != 0x05) {
        printf("[mcs51] FAIL: ADCCHS readback mismatch\n");
        return 1;
    }

    // 2. Verify that XSFR proxies (ADCLDO) are bound and functional
    ADCLDO = 0x80;
    if (static_cast<uint8_t>(ADCLDO) != 0x80) {
        printf("[mcs51] FAIL: ADCLDO XSFR readback mismatch\n");
        return 1;
    }

    // 3. Verify vendor mask macros (using non-constant comparison to avoid C4127)
    volatile uint8_t adgo_msk = ADC_ADCON0_ADGO_Msk;
    volatile uint8_t aden_msk = ADC_ADCON1_ADEN_Msk;
    if (adgo_msk != 0x02 || aden_msk != 0x80) {
        printf("[mcs51] FAIL: vendor mask macro mismatch\n");
        return 1;
    }

    printf("[mcs51] PASS: wink_mcu.h facade successfully routes to REG_CMS8S78XX.H\n");
    return 0;
}
