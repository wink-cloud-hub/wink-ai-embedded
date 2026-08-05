idf_component_register(
    SRCS
        "main.c"
        "../../../wink-micro-os/samples/avoidance_car/app_main.c"
        "../../../wink-micro-os/samples/avoidance_car/device_tree.c"
    INCLUDE_DIRS
        "."
        "../../../wink-micro-os/samples/avoidance_car"
        "../../../wink-micro-os/pal/include"
        "../../../wink-micro-os/dal/include"
        "../../../wink-micro-os/runtime/include"
        "../../../wink-micro-os/trace/include"
    REQUIRES
        driver
)

# Pass TARGET_PLATFORM definition
target_compile_definitions(${COMPONENT_LIB} PRIVATE TARGET_PLATFORM_esp32)

# Link wink-micro-os core libraries
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os ${CMAKE_BINARY_DIR}/wink-micro-os)
target_link_libraries(${COMPONENT_LIB} PRIVATE pal dal wink_runtime wink_trace)
