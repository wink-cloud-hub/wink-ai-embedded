# OSAL validation and resolution for WinkMicroOS
# Implements ADR-0041 validation matrix and resolves WINK_OSAL_TYPE

# Default value resolution
if(NOT DEFINED WINK_OSAL_TYPE OR "${WINK_OSAL_TYPE}" STREQUAL "")
    if(TARGET_PLATFORM STREQUAL "wasm")
        set(WINK_OSAL_TYPE "wasm" CACHE STRING "OSAL Type: wasm, host, freertos_esp32, baremetal" FORCE)
    elseif(TARGET_PLATFORM STREQUAL "host")
        set(WINK_OSAL_TYPE "host" CACHE STRING "OSAL Type: wasm, host, freertos_esp32, baremetal" FORCE)
    elseif(TARGET_PLATFORM STREQUAL "esp32")
        set(WINK_OSAL_TYPE "freertos_esp32" CACHE STRING "OSAL Type: wasm, host, freertos_esp32, baremetal" FORCE)
    else()
        # For any unknown platform, default to baremetal
        set(WINK_OSAL_TYPE "baremetal" CACHE STRING "OSAL Type: wasm, host, freertos_esp32, baremetal" FORCE)
    endif()
endif()

message(STATUS "[OSAL] TARGET_PLATFORM = ${TARGET_PLATFORM}")
message(STATUS "[OSAL] WINK_OSAL_TYPE  = ${WINK_OSAL_TYPE}")

# Validation function
function(wink_validate_osal_combo platform osal_type)
    set(is_valid FALSE)
    
    if(platform STREQUAL "wasm" AND osal_type STREQUAL "wasm")
        set(is_valid TRUE)
    elseif(platform STREQUAL "host" AND osal_type STREQUAL "host")
        set(is_valid TRUE)
    elseif(platform STREQUAL "esp32" AND osal_type STREQUAL "freertos_esp32")
        set(is_valid TRUE)
    endif()

    if(NOT is_valid)
        message(FATAL_ERROR 
            "\n[OSAL] FATAL: Illegal configuration combo!\n"
            "  TARGET_PLATFORM = '${platform}'\n"
            "  WINK_OSAL_TYPE  = '${osal_type}'\n\n"
            "  For Phase A, only the following combinations are allowed:\n"
            "    - TARGET_PLATFORM=wasm  × WINK_OSAL_TYPE=wasm\n"
            "    - TARGET_PLATFORM=host  × WINK_OSAL_TYPE=host\n"
            "    - TARGET_PLATFORM=esp32 × WINK_OSAL_TYPE=freertos_esp32\n\n"
            "  If you recently changed TARGET_PLATFORM, please clear your CMake cache:\n"
            "    cmake -U WINK_OSAL_TYPE <build-dir>  or delete the build directory.\n"
        )
    endif()
endfunction()

# Enforce validation on configure
wink_validate_osal_combo("${TARGET_PLATFORM}" "${WINK_OSAL_TYPE}")
