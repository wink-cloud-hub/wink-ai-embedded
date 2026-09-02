# pdk_button_led — production PDK (PFS154 / PMS150C) wasm-sim app
#
# Builds the PDK C firmware using SDCC and links with UniSim PDK virtual core.

get_filename_component(_PDK_APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(_PDK_SRC "${CMAKE_CURRENT_SOURCE_DIR}/pdk_button_led.c")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})
set(GEN_HEX "${GEN_DIR}/pdk_button_led.hex")

find_program(SDCC_EXECUTABLE NAMES sdcc HINTS "D:/software-install/sdcc/bin")
find_program(PACKIHX_EXECUTABLE NAMES packihx HINTS "D:/software-install/sdcc/bin")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/wink-app.json" _pdk_app_json_raw LIMIT 2048)
if(_pdk_app_json_raw MATCHES "\"mcu\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"")
    string(TOUPPER "${CMAKE_MATCH_1}" _PDK_MCU_DEF)
else()
    set(_PDK_MCU_DEF "PFS154")
endif()

if(SDCC_EXECUTABLE AND PACKIHX_EXECUTABLE)
    set(PDK_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/vendors/PDK/pdk-includes")
    add_custom_command(
        OUTPUT ${GEN_HEX}
        COMMAND ${SDCC_EXECUTABLE} -mpdk14 -D${_PDK_MCU_DEF} -I${PDK_INCLUDES} ${_PDK_SRC} -o ${GEN_DIR}/pdk_button_led.ihx
        COMMAND ${PACKIHX_EXECUTABLE} ${GEN_DIR}/pdk_button_led.ihx > ${GEN_HEX}
        DEPENDS ${_PDK_SRC}
        COMMENT "Compiling PDK firmware: pdk_button_led.c -> pdk_button_led.hex (-D${_PDK_MCU_DEF})"
        VERBATIM
    )
endif()

message(STATUS "pdk_button_led: registered for UniSim Wasm / Hex simulation.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
