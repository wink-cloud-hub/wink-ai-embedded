/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_SOC_CAPS_EVAL_H
#define WINK_H_GUARD_SOC_SOC_CAPS_EVAL_H
#ifndef __WINK_HARVESTED_SOC_SOC_CAPS_EVAL_H__
#define __WINK_HARVESTED_SOC_SOC_CAPS_EVAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SOC_HAS
#define SOC_HAS(_module) SOC_ ## _module ## _SUPPORTED
#endif
#ifndef SOC_IS
#define SOC_IS(_target) _SOC_CAPS_EVAL(TARGET_IS_ ## _target)
#endif
#ifndef SOC_MODULE_ATTR
#define SOC_MODULE_ATTR(_module, _attr) _SOC_CAPS_EVAL(_module ## _ ## _attr)
#endif
#ifndef SOC_MODULE_SUPPORT
#define SOC_MODULE_SUPPORT(_module, _feat) _SOC_CAPS_EVAL(_module ## _SUPPORT_ ## _feat)
#endif
#ifndef _SOC_CAPS_EVAL
#define _SOC_CAPS_EVAL(_name) _SOC_CAPS_ ## _name
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_SOC_CAPS_EVAL_H__ */
#endif /* WINK_H_GUARD_SOC_SOC_CAPS_EVAL_H */
