# OSAL sub-directory CMakeLists.txt
# Exports WINK_OSAL_SOURCES and WINK_OSAL_INCLUDE_DIRS to parent scope based on WINK_OSAL_TYPE

set(OSAL_SOURCES)
set(OSAL_INCLUDE_DIRS)

if(WINK_OSAL_TYPE STREQUAL "wasm")
    list(APPEND OSAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/wasm/pal_osal_wasm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/wasm/sim_ctx_emscripten_fiber.c
        ${CMAKE_CURRENT_SOURCE_DIR}/common/pal_osal_ringbuf.c
    )
    list(APPEND OSAL_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
        # T-DEP-01: Temporary access to wasm target headers. To be removed in Phase B.
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
    )
elseif(WINK_OSAL_TYPE STREQUAL "host")
    list(APPEND OSAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/host/pal_osal_host.c
        ${CMAKE_CURRENT_SOURCE_DIR}/host/sim_ctx_win32_fiber.c
        ${CMAKE_CURRENT_SOURCE_DIR}/common/pal_osal_ringbuf.c
    )
    list(APPEND OSAL_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/host
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../test/stubs
    )
elseif(WINK_OSAL_TYPE STREQUAL "freertos_esp32")
    list(APPEND OSAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_esp32/pal_osal_freertos_esp32.c
    )
    list(APPEND OSAL_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/freertos_esp32
    )
elseif(WINK_OSAL_TYPE STREQUAL "baremetal")
    list(APPEND OSAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/baremetal/pal_osal_baremetal.c
    )
    list(APPEND OSAL_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/baremetal
    )
else()
    message(FATAL_ERROR "[OSAL] Unsupported WINK_OSAL_TYPE: ${WINK_OSAL_TYPE}")
endif()

# Export variables to parent scope
set(WINK_OSAL_SOURCES ${OSAL_SOURCES} PARENT_SCOPE)
set(WINK_OSAL_INCLUDE_DIRS ${OSAL_INCLUDE_DIRS} PARENT_SCOPE)
