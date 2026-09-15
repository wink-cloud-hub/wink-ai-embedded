// SPDX-License-Identifier: GPL-3.0-only
// AT89C52 classic-family private state (Stage2 S2-1, PLAN-20260911-MCS51-S2).
//
// Placeholder: classic is the zero-extension pure core — it currently owns
// NO private state (external MOVX bus tracking is the generic extbus concept
// and stays in core, see S2-1 notes). The header exists so every family has
// a uniform protocol surface (cf. cms8s_priv.h); classic contexts bind
// soc_priv = nullptr explicitly. If classic ever gains private state, its
// pool + binder land here following the cms8s pattern.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t reserved;  // nonzero size: never empty-struct (MISRA-style)
} At89Priv;

#ifdef __cplusplus
}  // extern "C"
#endif
