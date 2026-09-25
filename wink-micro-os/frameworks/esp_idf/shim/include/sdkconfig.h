/* SPDX-License-Identifier: LGPL-3.0-only */
/* 默认 sdkconfig 垫片（手写豁免，通道 A）：无 corpus overlay 时提供基础层。
 * 本目录在 include 搜索顺序中刻意排在最后（见 esp_idf_sources.cmake 与
 * test/wasm/esp_idf_wasm_compile.cmake），保证 `test/corpus/<sample>/include/sdkconfig.h`
 * 的 overlay 优先命中；quoted include 的 includer-relative 搜索不会命中 include/。 */
#ifndef WINK_SDKCONFIG_SHIM_H_
#define WINK_SDKCONFIG_SHIM_H_

#include "sdkconfig_base.h"

#endif /* WINK_SDKCONFIG_SHIM_H_ */
