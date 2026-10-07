# SPDX-License-Identifier: LGPL-3.0-only
if(ESP_PLATFORM)
    return()
endif()

include(esp_idf_sources.cmake)

add_library(wink_framework_esp_idf STATIC EXCLUDE_FROM_ALL
    ${ESP_IDF_FRAMEWORK_SOURCES}
)

target_include_directories(wink_framework_esp_idf PUBLIC
    ${ESP_IDF_FRAMEWORK_INCLUDES}
)

target_include_directories(wink_framework_esp_idf PRIVATE
    ${ESP_IDF_FRAMEWORK_PRIVATE_INCLUDES}
)

# SoC selection macro SSOT (ADR-0085 D3 / ADR-0087) and profile capacity definitions (ISSUE-03)
target_compile_definitions(wink_framework_esp_idf PUBLIC
    ${WINK_IDF_TARGET_DEFINE}=1
    "${WINK_IDF_TARGET_STRING_DEFINE}"
    ${WINK_ESP_PROFILE_DEFINES}
    SIMULATION=1
    WINK_SIM_TEST=1
)

if(TARGET pal_host)
    target_compile_definitions(pal_host PRIVATE WINK_SIM_MAX_TASKS=${WINK_SIM_MAX_TASKS})
endif()
if(TARGET pal_wasm)
    target_compile_definitions(pal_wasm PRIVATE WINK_SIM_MAX_TASKS=${WINK_SIM_MAX_TASKS})
endif()

# Pointer safety and warning gate (ISSUE-10)
if(MSVC)
    target_compile_options(wink_framework_esp_idf PRIVATE /W4 /WX /wd4100 /wd4210 /wd4201 /we4311 /we4312)
else()
    target_compile_options(wink_framework_esp_idf PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-unused-result -Wpointer-to-int-cast -Wint-to-pointer-cast)
endif()

target_link_libraries(wink_framework_esp_idf PUBLIC
    pal
)

if(TARGET generate_config)
    add_dependencies(wink_framework_esp_idf generate_config)
endif()

