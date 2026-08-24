# Resolve WINK_TOOLS_ROOT or WINK_CLI_EXECUTABLE.
#
# Requires WINK_MICRO_OS_ROOT when included from nested CMakeLists; otherwise
# defaults to the parent of this cmake/ directory.

if(NOT DEFINED WINK_MICRO_OS_ROOT OR WINK_MICRO_OS_ROOT STREQUAL "")
    get_filename_component(WINK_MICRO_OS_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

# 1. Check for WINK_CLI_EXECUTABLE in PATH
find_program(WINK_CLI_EXECUTABLE NAMES winkcli wink)

# 2. Check for WINK_TOOLS_ROOT in ENV, Cache, or Sibling Workspace
if(DEFINED ENV{WINK_TOOLS_ROOT} AND NOT "$ENV{WINK_TOOLS_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{WINK_TOOLS_ROOT}" WINK_TOOLS_ROOT)
elseif(NOT DEFINED WINK_TOOLS_ROOT OR WINK_TOOLS_ROOT STREQUAL "")
    get_filename_component(_WINK_WS "${WINK_MICRO_OS_ROOT}/.." ABSOLUTE)
    if(EXISTS "${_WINK_WS}/wink-tools/tools/codegen/scripts/list_drivers.py")
        set(WINK_TOOLS_ROOT "${_WINK_WS}/wink-tools")
    elseif(EXISTS "${_WINK_WS}/packages/wink-tools/tools/codegen/scripts/list_drivers.py")
        set(WINK_TOOLS_ROOT "${_WINK_WS}/packages/wink-tools")
    elseif(EXISTS "${_WINK_WS}/../wink-ai/packages/wink-tools/tools/codegen/scripts/list_drivers.py")
        set(WINK_TOOLS_ROOT "${_WINK_WS}/../wink-ai/packages/wink-tools")
    endif()
endif()

if(DEFINED WINK_TOOLS_ROOT AND NOT WINK_TOOLS_ROOT STREQUAL "")
    get_filename_component(WINK_TOOLS_ROOT "${WINK_TOOLS_ROOT}" ABSOLUTE)
endif()

if((NOT DEFINED WINK_TOOLS_ROOT OR NOT EXISTS "${WINK_TOOLS_ROOT}/tools/codegen/scripts/list_drivers.py") AND NOT WINK_CLI_EXECUTABLE)
    message(FATAL_ERROR
        "Wink toolchain not found!\n"
        "  - For source mode: Set WINK_TOOLS_ROOT to wink-tools source directory.\n"
        "  - For CLI binary mode: Run 'pip install winkcli' to install the global toolchain.")
endif()

# ADR-0051: build truth for extra codegen extension roots. Env WINK_CODEGEN_PATHS
# is CLI convenience only — configure always passes this cache into list_drivers.
set(WINK_CODEGEN_PATHS "" CACHE STRING
    "Extra codegen extension roots (os.pathsep or comma-separated). CMake cache is build truth; env is CLI-only.")
