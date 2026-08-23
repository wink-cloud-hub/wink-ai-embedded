# test_dal_pruning_neg.cmake — compile-negative test invoked as `cmake -P`.
#
# Asserts:
#   1. Compiling test_dal_pruning_neg.c with -DWINK_USE_RC_SERVO=0 FAILS.
#   2. The compiler's stderr contains the friendly remediation hint from
#      WINK_UNAVAILABLE_MSG.
#
# Input variables (passed via -D from add_test):
#   GCC         — path to the C compiler (${CMAKE_C_COMPILER}).
#   SRC_DIR     — directory containing test_dal_pruning_neg.c.
#   DAL_INC     — path to dal/include (for dal_rc_servo.h).
#   PAL_INC     — path to pal/include (for wink_status.h).
#   WINK_STATUS_H — wink_status.h (unused; kept for diagnostic clarity).

if(NOT GCC)
    message(FATAL_ERROR "test_dal_pruning_neg: -DGCC=... is required")
endif()
if(NOT SRC_DIR)
    message(FATAL_ERROR "test_dal_pruning_neg: -DSRC_DIR=... is required")
endif()

set(_src "${SRC_DIR}/test_dal_pruning_neg.c")
set(_inc_flags
    "-I${DAL_INC}"
    "-I${DAL_INC}/actuator"
    "-I${PAL_INC}"
)

if(MSVC OR GCC MATCHES "cl(\\.exe)?$")
    set(_warn_flags "/Zs" "/we4996")
else()
    set(_warn_flags "-fsyntax-only" "-Wall" "-Wextra" "-Werror")
endif()

# Run the compiler; expect non-zero exit.
execute_process(
    COMMAND "${GCC}"
            ${_inc_flags}
            -DWINK_USE_RC_SERVO=0
            ${_warn_flags}
            "${_src}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(_rc EQUAL 0)
    message(FATAL_ERROR
        "test_dal_pruning_neg: expected COMPILE FAILURE with -DWINK_USE_RC_SERVO=0, "
        "but gcc exited 0. WINK_UNAVAILABLE_MSG is NOT firing.")
endif()

# The friendly message should appear in stdout or stderr (MSVC emits to stdout).
set(_combined_out "${_stdout}\n${_stderr}")
string(FIND "${_combined_out}" "RC servo driver not enabled" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR
        "test_dal_pruning_neg: compiler failed (rc=${_rc}) but the expected "
        "remediation hint \"RC servo driver not enabled\" was NOT found.\n"
        "--- output was ---\n${_combined_out}\n"
        "--- end output ---")
endif()

message(STATUS "test_dal_pruning_neg: PASS — unavailable error fired with remediation hint.")
