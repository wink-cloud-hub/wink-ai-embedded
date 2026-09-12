# frameworks/CMakeLists.txt
# Dispatcher for optional frameworks (Arduino, MCS-51 interception, etc.)

add_subdirectory(arduino)

# MCS-51/8051 zero-code sim interception (Axis B, ADR-0070). Registers the
# core/chip/device split targets; they are EXCLUDE_FROM_ALL and self-skip on
# ESP_PLATFORM, so adding this unconditionally is free for non-mcs51 builds.
add_subdirectory(mcs51)
