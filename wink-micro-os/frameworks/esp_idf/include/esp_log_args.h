/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_ARGS_H
#define WINK_H_GUARD_ESP_LOG_ARGS_H
#ifndef __WINK_HARVESTED_ESP_LOG_ARGS_H__
#define __WINK_HARVESTED_ESP_LOG_ARGS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "esp_assert.h"
#include "esp_log_config.h"
#include "esp_macros.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_LOG_ARGS
#define ESP_LOG_ARGS(...) , ##__VA_ARGS__
#endif
#ifndef ESP_LOG_ARGS_TYPE_LEN
#define ESP_LOG_ARGS_TYPE_LEN (2)
#endif
#ifndef ESP_LOG_ARGS_TYPE_MASK
#define ESP_LOG_ARGS_TYPE_MASK ((1 << ESP_LOG_ARGS_TYPE_LEN) - 1)
#endif
#ifndef ESP_LOG_DETECT_TYPE
#define ESP_LOG_DETECT_TYPE(arg) (  _Generic((arg),  esp_log_args_end_t: ESP_LOG_ARGS_TYPE_NONE,  char*: ESP_LOG_ARGS_TYPE_POINTER,  const char*: ESP_LOG_ARGS_TYPE_POINTER,  uint8_t*: ESP_LOG_ARGS_TYPE_POINTER,  const uint8_t*: ESP_LOG_ARGS_TYPE_POINTER,  long long int: ESP_LOG_ARGS_TYPE_64BITS,  long long unsigned int: ESP_LOG_ARGS_TYPE_64BITS,  double: ESP_LOG_ARGS_TYPE_64BITS,  float: ESP_LOG_ARGS_TYPE_64BITS,   default: ESP_LOG_ARGS_TYPE_32BITS  ))
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE
#define ESP_LOG_INIT_ARG_TYPE(n, ...) ESP_LOG_INIT_ARG_TYPE_N(n)(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_1
#define ESP_LOG_INIT_ARG_TYPE_1(a) ESP_LOG_PACK_4_TYPES(a, (esp_log_args_end_t){0}, (esp_log_args_end_t){0}, (esp_log_args_end_t){0})
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_10
#define ESP_LOG_INIT_ARG_TYPE_10(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_6(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_11
#define ESP_LOG_INIT_ARG_TYPE_11(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_7(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_12
#define ESP_LOG_INIT_ARG_TYPE_12(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_8(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_13
#define ESP_LOG_INIT_ARG_TYPE_13(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_9(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_14
#define ESP_LOG_INIT_ARG_TYPE_14(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_10(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_15
#define ESP_LOG_INIT_ARG_TYPE_15(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_11(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_16
#define ESP_LOG_INIT_ARG_TYPE_16(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_12(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_17
#define ESP_LOG_INIT_ARG_TYPE_17(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_13(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_18
#define ESP_LOG_INIT_ARG_TYPE_18(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_14(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_19
#define ESP_LOG_INIT_ARG_TYPE_19(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_15(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_2
#define ESP_LOG_INIT_ARG_TYPE_2(a, b) ESP_LOG_PACK_4_TYPES(a, b, (esp_log_args_end_t){0}, (esp_log_args_end_t){0})
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_20
#define ESP_LOG_INIT_ARG_TYPE_20(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_16(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_21
#define ESP_LOG_INIT_ARG_TYPE_21(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_17(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_22
#define ESP_LOG_INIT_ARG_TYPE_22(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_18(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_23
#define ESP_LOG_INIT_ARG_TYPE_23(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_19(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_24
#define ESP_LOG_INIT_ARG_TYPE_24(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_20(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_25
#define ESP_LOG_INIT_ARG_TYPE_25(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_21(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_26
#define ESP_LOG_INIT_ARG_TYPE_26(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_22(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_27
#define ESP_LOG_INIT_ARG_TYPE_27(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_23(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_28
#define ESP_LOG_INIT_ARG_TYPE_28(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_24(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_29
#define ESP_LOG_INIT_ARG_TYPE_29(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_25(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_3
#define ESP_LOG_INIT_ARG_TYPE_3(a, b, c) ESP_LOG_PACK_4_TYPES(a, b, c, (esp_log_args_end_t){0})
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_30
#define ESP_LOG_INIT_ARG_TYPE_30(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_26(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_31
#define ESP_LOG_INIT_ARG_TYPE_31(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_27(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_32
#define ESP_LOG_INIT_ARG_TYPE_32(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_28(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_33
#define ESP_LOG_INIT_ARG_TYPE_33(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_29(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_34
#define ESP_LOG_INIT_ARG_TYPE_34(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_30(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_35
#define ESP_LOG_INIT_ARG_TYPE_35(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_31(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_36
#define ESP_LOG_INIT_ARG_TYPE_36(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_32(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_37
#define ESP_LOG_INIT_ARG_TYPE_37(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_33(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_38
#define ESP_LOG_INIT_ARG_TYPE_38(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_34(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_39
#define ESP_LOG_INIT_ARG_TYPE_39(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_35(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_4
#define ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d) ESP_LOG_PACK_4_TYPES(a, b, c, d)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_40
#define ESP_LOG_INIT_ARG_TYPE_40(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_36(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_41
#define ESP_LOG_INIT_ARG_TYPE_41(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_37(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_42
#define ESP_LOG_INIT_ARG_TYPE_42(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_38(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_43
#define ESP_LOG_INIT_ARG_TYPE_43(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_39(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_44
#define ESP_LOG_INIT_ARG_TYPE_44(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_40(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_45
#define ESP_LOG_INIT_ARG_TYPE_45(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_41(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_46
#define ESP_LOG_INIT_ARG_TYPE_46(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_42(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_47
#define ESP_LOG_INIT_ARG_TYPE_47(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_43(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_48
#define ESP_LOG_INIT_ARG_TYPE_48(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_44(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_49
#define ESP_LOG_INIT_ARG_TYPE_49(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_45(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_5
#define ESP_LOG_INIT_ARG_TYPE_5(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_1(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_50
#define ESP_LOG_INIT_ARG_TYPE_50(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_46(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_51
#define ESP_LOG_INIT_ARG_TYPE_51(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_47(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_52
#define ESP_LOG_INIT_ARG_TYPE_52(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_48(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_53
#define ESP_LOG_INIT_ARG_TYPE_53(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_49(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_54
#define ESP_LOG_INIT_ARG_TYPE_54(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_50(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_55
#define ESP_LOG_INIT_ARG_TYPE_55(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_51(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_56
#define ESP_LOG_INIT_ARG_TYPE_56(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_52(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_57
#define ESP_LOG_INIT_ARG_TYPE_57(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_53(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_58
#define ESP_LOG_INIT_ARG_TYPE_58(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_54(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_59
#define ESP_LOG_INIT_ARG_TYPE_59(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_55(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_6
#define ESP_LOG_INIT_ARG_TYPE_6(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_2(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_60
#define ESP_LOG_INIT_ARG_TYPE_60(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_56(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_7
#define ESP_LOG_INIT_ARG_TYPE_7(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_3(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_8
#define ESP_LOG_INIT_ARG_TYPE_8(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_4(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_9
#define ESP_LOG_INIT_ARG_TYPE_9(a, b, c, d, ...) ESP_LOG_INIT_ARG_TYPE_4(a, b, c, d), ESP_LOG_INIT_ARG_TYPE_5(__VA_ARGS__)
#endif
#ifndef ESP_LOG_INIT_ARG_TYPE_N
#define ESP_LOG_INIT_ARG_TYPE_N(n) ESP_LOG_INIT_ARG_TYPE_##n
#endif
#ifndef ESP_LOG_PACK_4_TYPES
#define ESP_LOG_PACK_4_TYPES(a, b, c, d) (char) (  (ESP_LOG_DETECT_TYPE(a) << (0 * ESP_LOG_ARGS_TYPE_LEN)) |  (ESP_LOG_DETECT_TYPE(b) << (1 * ESP_LOG_ARGS_TYPE_LEN)) |  (ESP_LOG_DETECT_TYPE(c) << (2 * ESP_LOG_ARGS_TYPE_LEN)) |  (ESP_LOG_DETECT_TYPE(d) << (3 * ESP_LOG_ARGS_TYPE_LEN)))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_LOG_ARGS_TYPE_NONE = 0,
    ESP_LOG_ARGS_TYPE_32BITS = 1,
    ESP_LOG_ARGS_TYPE_64BITS = 2,
    ESP_LOG_ARGS_TYPE_POINTER = 3,
} esp_log_args_type_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    unsigned tmp;
} esp_log_args_end_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_ARGS_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_ARGS_H */
