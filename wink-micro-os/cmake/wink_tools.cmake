# Resolve WINK_TOOLS_ROOT (sibling wink-tools/ in monorepo, or env/cache override).
#
# Requires WINK_MICRO_OS_ROOT when included from nested CMakeLists; otherwise
# defaults to the parent of this cmake/ directory.

if(NOT DEFINED WINK_MICRO_OS_ROOT OR WINK_MICRO_OS_ROOT STREQUAL "")
    get_filename_component(WINK_MICRO_OS_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if(DEFINED ENV{WINK_TOOLS_ROOT} AND NOT "$ENV{WINK_TOOLS_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{WINK_TOOLS_ROOT}" WINK_TOOLS_ROOT)
elseif(NOT DEFINED WINK_TOOLS_ROOT OR WINK_TOOLS_ROOT STREQUAL "")
    get_filename_component(_WINK_WS "${WINK_MICRO_OS_ROOT}/.." ABSOLUTE)
    set(WINK_TOOLS_ROOT "${_WINK_WS}/wink-tools")
endif()
get_filename_component(WINK_TOOLS_ROOT "${WINK_TOOLS_ROOT}" ABSOLUTE)

if(NOT EXISTS "${WINK_TOOLS_ROOT}/tools/codegen/list_drivers.py")
    message(FATAL_ERROR
        "WINK_TOOLS_ROOT='${WINK_TOOLS_ROOT}' is invalid "
        "(missing tools/codegen/list_drivers.py). Set WINK_TOOLS_ROOT or place "
        "wink-tools/ next to wink-micro-os/.")
endif()

# ADR-0051: build truth for extra codegen extension roots. Env WINK_CODEGEN_PATHS
# is CLI convenience only — configure always passes this cache into list_drivers.
set(WINK_CODEGEN_PATHS "" CACHE STRING
    "Extra codegen extension roots (os.pathsep or comma-separated). CMake cache is build truth; env is CLI-only.")
