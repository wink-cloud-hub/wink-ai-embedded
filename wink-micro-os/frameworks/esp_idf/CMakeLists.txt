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
    ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
)

target_link_libraries(wink_framework_esp_idf PUBLIC
    pal
)

if(TARGET generate_config)
    add_dependencies(wink_framework_esp_idf generate_config)
endif()

