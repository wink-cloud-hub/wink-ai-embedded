/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_LIBUNWIND_H
#define WINK_H_GUARD_LIBUNWIND_H
#ifndef __WINK_HARVESTED_LIBUNWIND_H__
#define __WINK_HARVESTED_LIBUNWIND_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef UNW_EBADREG
#define UNW_EBADREG 3
#endif
#ifndef UNW_EBADVERSION
#define UNW_EBADVERSION 9
#endif
#ifndef UNW_EINVAL
#define UNW_EINVAL 8
#endif
#ifndef UNW_ENOINFO
#define UNW_ENOINFO 10
#endif
#ifndef UNW_ESTOPUNWIND
#define UNW_ESTOPUNWIND 5
#endif
#ifndef UNW_ESUCCESS
#define UNW_ESUCCESS 0
#endif
#ifndef UNW_EUNSPEC
#define UNW_EUNSPEC 1
#endif
#ifndef UNW_UNKNOWN_TARGET
#define UNW_UNKNOWN_TARGET 1
#endif
#ifndef unw_getcontext
#define unw_getcontext(ctx) ({  int retval;  if (ctx == NULL) {  retval = -UNW_EUNSPEC;  } else {  UNW_GET_CONTEXT(ctx);  retval = UNW_ESUCCESS;  }  retval;  })
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void * ExecutionFrame;
typedef ExecutionFrame unw_context_t;
typedef uint32_t unw_regnum_t;
typedef unw_context_t unw_cursor_t;
typedef unsigned long unw_word_t;
typedef void * unw_addr_space_t;
typedef void * unw_fpreg_t;



#if defined(__WINK_SIM__)
int unw_get_reg(unw_cursor_t* cp, unw_regnum_t reg, unw_word_t* valp) WINK_SLA_ERROR("Wink SLA Violation: unw_get_reg out of Core 8 scope.");
#else
int unw_get_reg(unw_cursor_t* cp, unw_regnum_t reg, unw_word_t* valp);
#endif

#if defined(__WINK_SIM__)
int unw_init_local(unw_cursor_t* c, unw_context_t* ctx) WINK_SLA_ERROR("Wink SLA Violation: unw_init_local out of Core 8 scope.");
#else
int unw_init_local(unw_cursor_t* c, unw_context_t* ctx);
#endif

#if defined(__WINK_SIM__)
int unw_set_reg(unw_cursor_t* cp, unw_regnum_t reg, unw_word_t val) WINK_SLA_ERROR("Wink SLA Violation: unw_set_reg out of Core 8 scope.");
#else
int unw_set_reg(unw_cursor_t* cp, unw_regnum_t reg, unw_word_t val);
#endif

#if defined(__WINK_SIM__)
int unw_step(unw_cursor_t* cp) WINK_SLA_ERROR("Wink SLA Violation: unw_step out of Core 8 scope.");
#else
int unw_step(unw_cursor_t* cp);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_LIBUNWIND_H__ */
#endif /* WINK_H_GUARD_LIBUNWIND_H */
