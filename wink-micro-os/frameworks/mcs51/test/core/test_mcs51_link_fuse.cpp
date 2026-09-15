// SPDX-License-Identifier: GPL-3.0-only
// Review hardening: link-time self-registration under-registration fuse.
//
// Release build (no WINK_MCS51_STRICT): a chip-model family
// (MCS51_CAP_CHIP_MODELS) reset against an emptied registry counts a miss
// (warn once), classic stays silent, and registering the package heals the
// link.
//
// STRICT build (same TU, STRICT-built core/cms8s): the same detection is
// process-fatal, verified via child re-execution (the parent asserts a
// non-zero child exit for the missing register OBJECT and a zero exit for a
// healthy link, proving the harness discriminates).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_peripheral.h"
#include "mcs51_test_harness.h"

#ifdef _MSC_VER
// No abort dialog in CI: fail fast with an exit code instead.
#include <crtdbg.h>
#endif

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#ifdef WINK_MCS51_STRICT

// ── STRICT: death-test policy via child re-execution ────────────────────────
int child_main(int case_id) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    Mcu51Context* ctx = mcs51_get_context();
    switch (case_id) {
        case 1:  // register OBJECT missing: must abort
            mcs51_peripheral_registry_reset();
            mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
            mcs51_context_reset(ctx);
            break;
        case 9:  // package registered: healthy link must survive
            mcs51_test_use_family(MCS51_FAMILY_CMS8S78XX);
            break;
        default:
            printf("[mcs51-link-fuse] FAIL: unknown child case %d\n", case_id);
            return 2;
    }
    return 0;
}

int run_child(const char* exe, int case_id, bool expect_abort) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child %d", exe, case_id);
    const int rc = system(cmd);
    if (expect_abort && rc == 0) {
        printf("[mcs51-link-fuse] FAIL: child %d should have aborted (rc=0)\n",
               case_id);
        return 1;
    }
    if (!expect_abort && rc != 0) {
        printf("[mcs51-link-fuse] FAIL: healthy child exited rc=%d\n", rc);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--child") == 0) {
        return child_main(atoi(argv[2]));
    }
    int fails = 0;
    fails += run_child(argv[0], 1, true);
    fails += run_child(argv[0], 9, false);
    if (fails) {
        return 1;
    }
    printf("[mcs51-link-fuse] PASS (STRICT): missing register OBJECT aborts, "
           "healthy link survives\n");
    return 0;
}

#else  // release

namespace {

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[mcs51-link-fuse] FAIL: %s\n", msg);
        ++g_fails;
    }
}

}  // namespace

int main(void) {
    Mcu51Context* ctx = mcs51_get_context();
    const uint32_t base = wink_mcs51_chip_models_missing_count();

    // A: simulate a hand-written link that pulls only the family archive:
    //    the chip register OBJECT's constructor never runs, registry empty.
    mcs51_peripheral_registry_reset();
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(ctx);
    check(wink_mcs51_chip_models_missing_count() == base + 1u,
          "A: chip family + empty registry counts one miss");

    // B: the counter tracks every bare reset (the warning itself is one-shot).
    mcs51_context_reset(ctx);
    check(wink_mcs51_chip_models_missing_count() == base + 2u,
          "B: every bare reset re-detects");

    // C: classic is pure core: an empty registry is the contract.
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_reset(ctx);
    check(wink_mcs51_chip_models_missing_count() == base + 2u,
          "C: pure-core family stays silent");

    // D: linking the register entry (harness re-register) heals the fuse.
    mcs51_test_use_family(MCS51_FAMILY_CMS8S78XX);
    check(wink_mcs51_chip_models_missing_count() == base + 2u,
          "D: registered chip package passes the fuse");

    // E: classic never demands chip models even with a package registered.
    mcs51_test_use_family(MCS51_FAMILY_CLASSIC);
    check(wink_mcs51_chip_models_missing_count() == base + 2u,
          "E: classic never demands chip models");

    if (g_fails) {
        return 1;
    }
    printf("[mcs51-link-fuse] PASS: under-registration fuse + counter\n");
    return 0;
}

#endif
