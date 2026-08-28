/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 M4 host test: whole-port RMW vs Level-2 pin-trap strong isolation
 * (data-plane SSOT §4.3, Zero False-Trigger Test Vectors; acceptance #3).
 *
 * The unmodified Keil sample (sfr_rmw_isolation_test.c) runs four vectors and
 * logs per-pin on_write edge counters into XDATA (XBYTE[0..7], snapshotted per
 * vector at XBYTE[0x20..0x3F]). This driver binds, via the framework post-init
 * hook, one edge counter trap to every P1 pin (counters stored in the same
 * XDATA shadow bytes the Keil code zeroes/snapshots) plus a Read-Pin trap on
 * P1.2 that emulates a button held LOW for vector 4.
 *
 * Assertions:
 *   V1 (P1|=0x01 from 0x00): pin0 = 1 edge, pins1..7 = 0
 *   V2 (P1&=~0x01 from 0x01): pin0 = 1 edge, pins1..7 = 0
 *   V3 (P1=0x00 / P1&=~0x01, zero delta): all pins = 0 edges
 *   V4 (P1&=~0x01 with pin2 externally low):
 *       - pin0 = 1 edge, pins1..7 = 0 (pin2 latch never bounced)
 *       - SFR shadow latch for P1 == 0xFE (RMW read the LATCH 0xFF, so the
 *         button's low level was NOT written back into bit2 — Read-Latch
 *         golden rule, quasi-bidirectional FET lock-up defence)
 *       - Keil whole-port Read-Pin view saw bit2 low (0xFB before RMW)
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_trap.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

/* SFR + XDATA shadows (C linkage, framework BSS). */
extern uint8_t wink_mcs51_sfr_shadow[256];
extern uint8_t wink_mcs51_xdata_shadow[65536];

#define P1_SFR_ADDR   0x90u
#define P1_PORT       1u
#define KEY_BIT       2u
#define RUN_TICKS     20u

/* Edge counter trap: count into the XDATA byte the Keil code itself polls
 * (XBYTE[pin]), so its per-vector zeroing/snapshotting sees the same counts. */
static void edge_count_trap(void *ctx, uint8_t level) {
    volatile uint8_t *counter = (volatile uint8_t *)ctx;
    (void)level;
    ++(*counter);
}

/* Vector 4 button: external pin level held LOW regardless of the latch. */
static uint8_t key_low_read_trap(void *ctx) {
    (void)ctx;
    return 0u;
}

/* Runs AFTER framework init (which does mcs51_trap_reset): bind traps that
 * must survive the init-time reset. */
static void bind_rmw_traps(void) {
    for (uint8_t pin = 0; pin < 8u; pin++) {
        mcs51_trap_register_write(P1_PORT, pin, edge_count_trap,
                                  (void *)&wink_mcs51_xdata_shadow[pin]);
    }
    mcs51_trap_register_read(P1_PORT, KEY_BIT, key_low_read_trap, NULL);
}

static int check_vector(int v, int fails) {
    for (uint8_t pin = 0; pin < 8u; pin++) {
        uint8_t got = wink_mcs51_xdata_shadow[0x0020u + (unsigned)v * 8u + pin];
        uint8_t want = (pin == 0u) ? 1u : 0u;
        if (got != want) {
            printf("[mcs51] FAIL: V%d pin%u edge count %u, want %u "
                   "(Zero False-Trigger)\n", v + 1, (unsigned)pin,
                   (unsigned)got, (unsigned)want);
            fails++;
        }
    }
    return fails;
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    mcs51_framework_set_post_init_hook(bind_rmw_traps);

    wink_status_t st = wink_runtime_run(cb, RUN_TICKS);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }

    int fails = 0;

    /* All four vectors completed. */
    if (wink_mcs51_xdata_shadow[0x0010u] != 1u ||
        wink_mcs51_xdata_shadow[0x0011u] != 1u ||
        wink_mcs51_xdata_shadow[0x0012u] != 1u ||
        wink_mcs51_xdata_shadow[0x0013u] != 1u) {
        printf("[mcs51] FAIL: not all vectors ran (done markers %u %u %u %u)\n",
               (unsigned)wink_mcs51_xdata_shadow[0x0010u],
               (unsigned)wink_mcs51_xdata_shadow[0x0011u],
               (unsigned)wink_mcs51_xdata_shadow[0x0012u],
               (unsigned)wink_mcs51_xdata_shadow[0x0013u]);
        return 1;
    }

    fails = check_vector(0, fails);
    fails = check_vector(1, fails);
    /* V3: zero-delta fast path — every pin must stay at 0 edges. */
    for (uint8_t pin = 0; pin < 8u; pin++) {
        uint8_t got = wink_mcs51_xdata_shadow[0x0020u + 2u * 8u + pin];
        if (got != 0u) {
            printf("[mcs51] FAIL: V3 zero-delta pin%u fired %u edges "
                   "(fast path must be silent)\n", (unsigned)pin,
                   (unsigned)got);
            fails++;
        }
    }
    fails = check_vector(3, fails);

    /* V4 latch protection: P1 latch must be 0xFE — bit2 still 1 despite the
     * externally-low button (RMW read the latch, never the pin). */
    uint8_t latch = wink_mcs51_sfr_shadow[P1_SFR_ADDR];
    if (latch != 0xFEu) {
        printf("[mcs51] FAIL: V4 P1 latch = 0x%02X, want 0xFE (Read-Latch "
               "violation: pin2 low written back into latch)\n",
               (unsigned)latch);
        fails++;
    }

    /* V4 Keil-side whole-port Read-Pin views: bit2 reconstructed low. */
    if (wink_mcs51_xdata_shadow[0x0015u] != 0xFBu) {
        printf("[mcs51] FAIL: V4 Read-Pin view before RMW = 0x%02X, want 0xFB "
               "(button low on bit2)\n",
               (unsigned)wink_mcs51_xdata_shadow[0x0015u]);
        fails++;
    }
    if (wink_mcs51_xdata_shadow[0x0014u] != 0xFAu) {
        printf("[mcs51] FAIL: V4 Read-Pin view after RMW = 0x%02X, want 0xFA "
               "(bit0 written low, button low on bit2)\n",
               (unsigned)wink_mcs51_xdata_shadow[0x0014u]);
        fails++;
    }

    mcs51_framework_set_post_init_hook(NULL);

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: SFR RMW isolation — 4 Zero False-Trigger vectors "
           "(set/clear/zero-delta/Read-Latch protection), P1 latch 0xFE\n");
    return 0;
}
