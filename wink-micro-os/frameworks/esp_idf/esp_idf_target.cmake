# SPDX-License-Identifier: LGPL-3.0-only
# Single source of truth for ESP-IDF simulation target selection (ADR-0085 D3 / ADR-0087).
# Input: WINK_ESP_TARGET (CMake cache, e.g. -DWINK_ESP_TARGET=esp32c3; default esp32).
# Output: WINK_ESP_TARGET_INCLUDE_DIR / WINK_IDF_TARGET_DEFINE / WINK_IDF_TARGET_STRING_DEFINE.
# Consumers: frameworks/esp_idf/esp_idf_sources.cmake, frameworks/esp_idf/CMakeLists.txt,
#            test/wasm/esp_idf_wasm_compile.cmake.

if(NOT DEFINED WINK_ESP_TARGET)
    set(WINK_ESP_TARGET "esp32")
endif()

string(TOUPPER "${WINK_ESP_TARGET}" WINK_ESP_TARGET_UPPER)

set(WINK_ESP_TARGET_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/chips/${WINK_ESP_TARGET}/include")
set(WINK_IDF_TARGET_DEFINE "CONFIG_IDF_TARGET_${WINK_ESP_TARGET_UPPER}")
set(WINK_IDF_TARGET_STRING_DEFINE "CONFIG_IDF_TARGET=\"${WINK_ESP_TARGET}\"")

if(NOT EXISTS "${WINK_ESP_TARGET_INCLUDE_DIR}/soc/soc_caps.h")
    message(FATAL_ERROR
        "WINK_ESP_TARGET='${WINK_ESP_TARGET}' has no SoC capability data at "
        "${WINK_ESP_TARGET_INCLUDE_DIR}/soc/soc_caps.h. Add chips/<soc>/include/soc/ or use a supported target "
        "(see frameworks/esp_idf/channels.json).")
endif()
