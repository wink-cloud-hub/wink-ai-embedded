# DAL (Device Abstraction Layer) Component CMake
#
# P2-1 (2026-07-06): per-driver compile-time pruning via WINK_USE_XXX options.
# Each driver can be compiled out independently; when disabled its public
# header emits WINK_UNAVAILABLE_MSG stubs so callers get a friendly
# compile error pointing them to add the device to wink-app.json (instead
# of an opaque "undefined reference" at link time).
#
# How it works:
#   * WINK_USE_<XXX> options default to ON (backward compat).
#   * For each enabled driver, we:
#       (a) compile its .c file into the dal static library;
#       (b) PUBLIC-ly define WINK_USE_<XXX>=1 so that consumers of `dal`
#           see the real declarations in the header instead of the
#           unavailable stubs (headers use `#if !defined(WINK_USE_X)
#           || !WINK_USE_X` to switch between real decls and stubs).
#   * When a driver is OFF, its .c is omitted from the library and no
#     WINK_USE_X=1 is defined, so consumers see the stubs.
#
# Defaults: all drivers ON (backward compatible). App-specific builds
# include() the codegen-produced app_options.cmake before add_subdirectory(dal)
# to force ON only the drivers declared in wink-app.json. Drivers that are
# neither forced ON nor explicitly defaulted-OFF here fall through to ON
# via the option() defaults below — so adding a new driver requires three
# edits:
#   1) Add an `option(WINK_USE_<XXX> ...)` line in the options block.
#   2) Add its source/header + compile_definitions in the enable block.
#   3) Add the WINK_UNAVAILABLE_MSG stub block at the bottom of its header.
add_library(dal STATIC)

# ── Per-driver compile options (default ON for backward compat) ────────
option(WINK_USE_LED        "Enable DAL LED driver"        ON)
option(WINK_USE_BUTTON     "Enable DAL Button driver"     ON)
option(WINK_USE_SERVO      "Enable DAL Servo driver"      ON)
option(WINK_USE_SSD1306    "Enable DAL SSD1306 OLED"      ON)
set(WINK_SSD1306_FONT "ascii_upper" CACHE STRING
    "SSD1306 5x7 font when WINK_USE_SSD1306=ON: minimal | ascii_upper")
option(WINK_USE_ULTRASONIC "Enable DAL Ultrasonic sensor" ON)
option(WINK_USE_GPS        "Enable DAL GPS driver"        ON)
option(WINK_USE_EEPROM     "Enable DAL EEPROM driver"     ON)

# ── Public include paths (always exposed — headers are still needed to
#    provide the WINK_UNAVAILABLE_MSG stubs that give friendly errors
#    when the driver is compiled out). ──────────────────────────────────
target_include_directories(dal PUBLIC
    include
    include/input
    include/output
    include/actuator
    include/display
    include/sensor
    include/communication
    include/storage
)

# DAL 源码通过 `wink_pt_debug.h` 拿到 WINK_ASSERT_NONBLOCKING()（ADR-0017 层 3）。
# 该头存放在 runtime/include/（2026-07-04 层级修正：从 pal/include/wink_status.h 迁出）。
# 仅需 include 路径，不引入 link 依赖（extern wink_pt_in_context/wink_trace_fault 由
# 各 target 的 OSAL + wink_trace 提供，可执行链接期解析）。
target_include_directories(dal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include
)

# DAL 依赖 PAL
target_link_libraries(dal PUBLIC pal)

# Helper: enable one driver — add its sources + PUBLIC -DWINK_USE_<OPT>=1 so
# both the library's own .c files and downstream consumers see the real
# declarations instead of the unavailable stubs.
function(_wink_dal_enable opt opt_def cat stem)
    if(${opt})
        target_sources(dal PRIVATE
            include/${cat}/dal_${stem}.h
            src/${cat}/dal_${stem}.c
        )
        target_compile_definitions(dal PUBLIC ${opt_def}=1)
    endif()
endfunction()

# ── Per-driver enablement (conditional on WINK_USE_XXX) ────────────────
# NOTE: headers are listed alongside sources so IDEs index them; they are
# not compiled directly. Keep this block in sync with the option() block
# above AND add a WINK_UNAVAILABLE_MSG stub to the header when adding a
# new driver.

# input
_wink_dal_enable(WINK_USE_BUTTON     WINK_USE_BUTTON     input     button)
# output
_wink_dal_enable(WINK_USE_LED        WINK_USE_LED        output    led)
# actuator
_wink_dal_enable(WINK_USE_SERVO      WINK_USE_SERVO      actuator  servo)
# display
if(WINK_USE_SSD1306)
    target_sources(dal PRIVATE
        include/display/dal_ssd1306.h
        src/display/dal_ssd1306.c
        src/display/dal_ssd1306_font_internal.h
    )
    if(WINK_SSD1306_FONT STREQUAL "minimal")
        target_sources(dal PRIVATE src/display/dal_ssd1306_font_5x7_minimal.c)
        target_compile_definitions(dal PUBLIC WINK_SSD1306_FONT_MINIMAL=1)
    elseif(WINK_SSD1306_FONT STREQUAL "ascii_upper")
        target_sources(dal PRIVATE src/display/dal_ssd1306_font_5x7_ascii_upper.c)
        target_compile_definitions(dal PUBLIC WINK_SSD1306_FONT_ASCII_UPPER=1)
    else()
        message(FATAL_ERROR
            "WINK_SSD1306_FONT must be 'minimal' or 'ascii_upper', got '${WINK_SSD1306_FONT}'")
    endif()
    target_compile_definitions(dal PUBLIC WINK_USE_SSD1306=1)
else()
    # Header stubs only — no font TU linked.
endif()
# sensor
_wink_dal_enable(WINK_USE_ULTRASONIC WINK_USE_ULTRASONIC sensor    ultrasonic)
# communication
_wink_dal_enable(WINK_USE_GPS        WINK_USE_GPS        communication gps)
# storage
_wink_dal_enable(WINK_USE_EEPROM     WINK_USE_EEPROM     storage   eeprom)
