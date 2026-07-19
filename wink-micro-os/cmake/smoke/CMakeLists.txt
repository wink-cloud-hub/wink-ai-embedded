# Minimal configure smoke for ADR-0041 OSAL combo matrix.
# Expected: FATAL_ERROR when TARGET_PLATFORM × WINK_OSAL_TYPE is illegal.
cmake_minimum_required(VERSION 3.16)
project(wink_osal_matrix_smoke LANGUAGES NONE)

if(NOT DEFINED TARGET_PLATFORM OR NOT DEFINED WINK_OSAL_TYPE)
    message(FATAL_ERROR "Pass -DTARGET_PLATFORM=... -DWINK_OSAL_TYPE=...")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../wink-micro-os/cmake/wink_osal.cmake")
message(STATUS "[matrix-smoke] legal combo OK: ${TARGET_PLATFORM} x ${WINK_OSAL_TYPE}")
