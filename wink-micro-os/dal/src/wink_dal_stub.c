/**
 * @file wink_dal_stub.c
 * @brief Empty translation unit so the `dal` static library always links.
 *
 * DAL driver TUs are compile-time pruned to those referenced by the app's
 * wink-app.json (ADR-0039). A framework-managed app that drives the hardware
 * directly (e.g. the MCS-51 interception framework, ADR-0075, which bit-bangs
 * SFRs and uses no DAL drivers) prunes every driver off, leaving the `dal`
 * target with zero sources — CMake rejects a STATIC library with no sources.
 * This mirrors wink_bal_stub.c: a non-static placeholder keeps the archive
 * non-empty; nothing references it, so it is dead-stripped at final link.
 *
 * Copyright (c) 2026 Wink-AI.
 */

#if defined(__GNUC__) || defined(__clang__)
#  define WINK_DAL_USED __attribute__((used))
#elif defined(_MSC_VER)
#  define WINK_DAL_USED
#else
#  define WINK_DAL_USED
#endif

WINK_DAL_USED int wink_dal_stub_placeholder = 0;
