# test_dal_pruning_neg.cmake — compile-negative test invoked as `cmake -P`.
#
# Asserts:
#   1. Compiling test_dal_pruning_neg.c with -DWINK_USE_SERVO=0 FAILS.
#   2. The compiler's stderr contains the friendly remediation hint from
#      WINK_UNAVAILABLE_MSG.
#
# Input variables (passed via -D from add_test):
#   GCC         — path to the C compiler (${CMAKE_C_COMPILER}).
#   SRC_DIR     — directory containing test_dal_pruning_neg.c.
#   DAL_INC     — path to dal/include (for dal_servo.h).
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

# Run the compiler; expect non-zero exit.
execute_process(
    COMMAND "${GCC}"
            ${_inc_flags}
            -DWINK_USE_SERVO=0
            -fsyntax-only
            -Wall -Wextra -Werror
            "${_src}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(_rc EQUAL 0)
    message(FATAL_ERROR
        "test_dal_pruning_neg: expected COMPILE FAILURE with -DWINK_USE_SERVO=0, "
        "but gcc exited 0. WINK_UNAVAILABLE_MSG is NOT firing.")
endif()

# The friendly message should appear in stderr.
string(FIND "${_stderr}" "Servo driver not enabled" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR
        "test_dal_pruning_neg: gcc failed (rc=${_rc}) but the expected "
        "remediation hint \"Servo driver not enabled\" was NOT in stderr.\n"
        "--- stderr was ---\n${_stderr}\n"
        "--- end stderr ---")
endif()

message(STATUS "test_dal_pruning_neg: PASS — unavailable error fired with remediation hint.")
