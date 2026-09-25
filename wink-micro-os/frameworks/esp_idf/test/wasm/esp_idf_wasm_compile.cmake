# SPDX-License-Identifier: GPL-3.0-only
# Wasm compile-only gate for ESP-IDF simulation interception layer (M0/M1).
# Checks emcc compilation (-Wall -Wextra -Werror) without linking.

function(add_esp_idf_wasm_compile_check name source_file)
    if(NOT WINK_BUILD_WASM_TESTS)
        message(STATUS "[esp_idf_wasm] Skipped wasm compile check for ${name} (emcc not found)")
        return()
    endif()

    get_filename_component(_src_abs "${source_file}" ABSOLUTE)
    set(_out_obj "${CMAKE_CURRENT_BINARY_DIR}/wasm_compile_${name}.o")

    set(_extra_includes ${ARGN})
    set(_inc_args "")
    foreach(_inc IN LISTS _extra_includes)
        list(APPEND _inc_args "-I${_inc}")
    endforeach()

    add_custom_command(
        OUTPUT "${_out_obj}"
        COMMAND ${EMCC_EXECUTABLE} -c "${_src_abs}" -o "${_out_obj}"
            -Wall -Wextra -Werror -Wno-unused-parameter
            -DUNITY_SUPPORT_64=1
            -I${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/esp_idf/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/esp_idf/chips/esp32/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/esp_idf/src/freertos
            -I${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
            -I${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
            -I${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/unity
            ${_inc_args}
        DEPENDS "${_src_abs}"
        COMMENT "Wasm compile-only check: ${name}"
        VERBATIM
    )

    add_custom_target("esp_idf_wasm_compile_${name}_target" ALL
        DEPENDS "${_out_obj}"
    )

    add_test(NAME "esp_idf_wasm_compile_${name}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target "esp_idf_wasm_compile_${name}_target"
    )
    set_tests_properties("esp_idf_wasm_compile_${name}" PROPERTIES LABELS "esp_idf_wasm;wasm_compile")
endfunction()
