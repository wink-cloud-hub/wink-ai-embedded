if(NOT WIN32)
    message(FATAL_ERROR
        "wink-micro-os host target currently supports Windows only "
        "(uses Win32 Fiber via sim_ctx_win32_fiber.c). "
        "See ADR-0013 §host-platform-matrix; Linux/macOS support requires "
        "adding sim_ctx_posix_ucontext.c.")
endif()

# host first-class target (OBJECT library) — Provides complete PAL implementation for tests and samples.
add_library(pal_host OBJECT
    pal_hal_host.c
    pal_hal_adc_host.c
    pal_log_host.c
    ${WINK_OSAL_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    pal_storage_host.c
    ${PAL_COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_dev_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
)

target_link_libraries(pal_host PUBLIC pal)

target_include_directories(pal_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_OSAL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include)
