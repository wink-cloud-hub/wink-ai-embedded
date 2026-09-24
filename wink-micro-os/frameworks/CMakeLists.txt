# frameworks/CMakeLists.txt
# Dispatcher for optional frameworks (Arduino, MCS-51 interception, etc.)

add_subdirectory(arduino)

# MCS-51/8051 zero-code sim interception (Axis B, ADR-0070). Registers the
# core/chip/device split targets; they are EXCLUDE_FROM_ALL and self-skip on
# ESP_PLATFORM, so adding this unconditionally is free for non-mcs51 builds.
add_subdirectory(mcs51)

option(ENABLE_ESP_IDF_FRAMEWORK "Enable ESP-IDF simulation interception framework" ON)
# ESP-IDF C-ABI sim interception (Axis B). EXCLUDE_FROM_ALL + ESP_PLATFORM
# self-skip inside frameworks/esp_idf/CMakeLists.txt, mirroring mcs51.
if(ENABLE_ESP_IDF_FRAMEWORK)
    add_subdirectory(esp_idf)
endif()

