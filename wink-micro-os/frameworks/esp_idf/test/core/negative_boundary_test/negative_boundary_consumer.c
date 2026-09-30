/* SPDX-License-Identifier: GPL-3.0-only */
/**
 * Negative compile test consumer:
 * Attempts to include private framework header "freertos_sync.h".
 * Must fail to compile when internal headers are properly scoped to PRIVATE.
 */
#include "esp_err.h"
#include "freertos_sync.h"

int main(void) {
    return 0;
}
