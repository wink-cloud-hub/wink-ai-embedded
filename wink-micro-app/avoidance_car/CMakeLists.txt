# Avoidance car — L1 distance-events sample (ADR-0033).
# Shared boilerplate in ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

# ── Codegen: wink-app.json → device_tree.[hc] + app_options.cmake ──
set(WINK_APP_JSON     ${CMAKE_CURRENT_SOURCE_DIR}/wink-app.json)
if(NOT DEFINED WINK_CODEGEN_ROOT)
    set(WINK_CODEGEN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/tools/codegen)
endif()
set(WINK_CODEGEN_TOOL ${WINK_CODEGEN_ROOT}/app_codegen.py)
set(GEN_DIR           ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(GEN_DEVICE_TREE_H ${GEN_DIR}/device_tree.h)
set(GEN_DEVICE_TREE_C ${GEN_DIR}/device_tree.c)
set(GEN_APP_OPTIONS   ${GEN_DIR}/app_options.cmake)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND
     PROPERTY CMAKE_CONFIGURE_DEPENDS ${WINK_APP_JSON})

file(GLOB WINK_CODEGEN_TEMPLATES ${WINK_CODEGEN_ROOT}/templates/*.j2)
file(GLOB WINK_CODEGEN_DRIVERS   ${WINK_CODEGEN_ROOT}/drivers/*.py)

add_custom_command(
    OUTPUT ${GEN_DEVICE_TREE_H} ${GEN_DEVICE_TREE_C}
           ${GEN_APP_OPTIONS}
    COMMAND ${Python3_EXECUTABLE} ${WINK_CODEGEN_TOOL}
            --config ${WINK_APP_JSON}
            --out-dir ${GEN_DIR}
    DEPENDS ${WINK_APP_JSON} ${WINK_CODEGEN_TOOL}
            ${WINK_CODEGEN_TEMPLATES} ${WINK_CODEGEN_DRIVERS}
    COMMENT "Generating avoidance_car device tree from wink-app.json"
    VERBATIM
)

set(APP_SOURCES
    ${GEN_DEVICE_TREE_C}
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

if(EMSCRIPTEN)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${WINK_CODEGEN_TOOL}
                --config ${WINK_APP_JSON}
                --out-dir ${GEN_DIR}
        RESULT_VARIABLE _avoid_codegen_rc
    )
    if(NOT _avoid_codegen_rc EQUAL 0)
        message(FATAL_ERROR
            "avoidance_car codegen failed (rc=${_avoid_codegen_rc}); "
            "check ${WINK_APP_JSON}")
    endif()
    set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${GEN_DIR} PARENT_SCOPE)
    return()
endif()

set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)
set(WINK_APP_INCLUDE_DIRS ${GEN_DIR} PARENT_SCOPE)

if(NOT EXISTS "${wink-micro-os_SOURCE_DIR}/test/test_app_e2e.c")
    message(STATUS
        "avoidance_car: skipping host e2e (no ${wink-micro-os_SOURCE_DIR}/test/ harness)")
    return()
endif()

add_executable(app_avoidance_car_e2e
    ${APP_SOURCES}
    ${WINK_SAMPLE_RUNTIME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_avoidance_car_e2e.c
    $<TARGET_OBJECTS:pal_host>)
wink_sample_apply_common(app_avoidance_car_e2e)
wink_sample_link_common(app_avoidance_car_e2e)
target_link_libraries(app_avoidance_car_e2e PRIVATE dal)
target_include_directories(app_avoidance_car_e2e PRIVATE ${GEN_DIR})
add_test(NAME app_avoidance_car_e2e COMMAND app_avoidance_car_e2e)
