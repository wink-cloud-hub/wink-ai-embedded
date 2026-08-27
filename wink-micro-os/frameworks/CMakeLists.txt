# frameworks/CMakeLists.txt
# Dispatcher for optional frameworks (Arduino, MCS-51 interception, etc.)

add_subdirectory(arduino)

# MCS-51/8051 zero-code sim interception (Axis B, ADR-0070). Registers the
# wink_mcs51_compat target; it is EXCLUDE_FROM_ALL and self-skips on
# ESP_PLATFORM, so adding it unconditionally is free for non-mcs51 builds.
add_subdirectory(mcs51)
