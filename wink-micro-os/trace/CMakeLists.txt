# trace (Golden Trace) — First-class peer layer STATIC library.
# Consumed by runtime/dal/app (cross-cutting service). Zero dynamic allocation.
add_library(wink_trace STATIC
    src/wink_trace.c)

target_include_directories(wink_trace PUBLIC include)
target_link_libraries(wink_trace PUBLIC pal)
