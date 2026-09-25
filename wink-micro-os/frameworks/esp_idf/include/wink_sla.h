/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c — factual C-ABI only. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c */
#pragma once
/* wink_sla.h — Harvester 生成，开源仓只读消费（计划 §4.3.2） */
#if defined(__WINK_SIM__) && defined(__GNUC__)
#define WINK_SLA_ERROR(msg) __attribute__((error(msg)))
#elif defined(__WINK_SIM__) && defined(_MSC_VER)
/* v2.13 MSVC smoke 实测：suffix 位置的 `__declspec(deprecated)` 非法（须在声明符前）。
   MSVC 无 error 属性 → 置空宏，Fail-Loud 由「链接缺符号（未实现 API 无定义）+
   winkcli lint 前移阻断」保证；见计划 §4.3.2/§5.1。 */
#define WINK_SLA_ERROR(msg)
#else
#define WINK_SLA_ERROR(msg)
#endif
