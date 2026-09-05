// SPDX-License-Identifier: Apache-2.0
// Spike-S2 link/compile harness (throwaway). Provides the symbols the cleaned
// user TU references, and proves the ISR vector + renamed main exported with C
// linkage. Does NOT call wink_mcs51_user_main (it is an infinite bare-metal
// loop; runtime behavior is Spike-S1's domain).
#include <cstdint>
#include <cstdio>

extern "C" {
uint8_t s_sfr_shadow[256] = {0};

static void (*s_isr_table[8])(void);
void wink_mcs51_set_isr(uint8_t vec, void (*fn)(void)) {
    if (vec < 8) s_isr_table[vec] = fn;
}

// user-side symbols, must exist with undecorated C linkage
void wink_mcs51_user_main(void);
void wink_isr_vector_1(void);
}

int main(void) {
    // Static auto-registration ran before main(); vector 1 must be populated.
    int fail = 0;
    if (s_isr_table[1] != &wink_isr_vector_1) {
        printf("[s2] FAIL: ISR vector 1 not auto-registered (got %p want %p)\n",
               (void*)s_isr_table[1], (void*)&wink_isr_vector_1);
        fail++;
    }
    // Invoke the ISR once: it toggles P1.0 (shadow[0x90] bit0).
    s_sfr_shadow[0x90] = 0x00;
    wink_isr_vector_1();
    uint8_t after = s_sfr_shadow[0x90] & 0x01u;
    if (after != 0x01) {
        printf("[s2] FAIL: ISR toggle P1.0, shadow[0x90]=0x%02X\n", s_sfr_shadow[0x90]);
        fail++;
    }
    if (fail == 0)
        printf("[s2] PASS: WINK_ISR auto-reg + C linkage + sbit toggle OK "
               "(P1=0x%02X)\n", s_sfr_shadow[0x90]);
    return fail ? 1 : 0;
}
