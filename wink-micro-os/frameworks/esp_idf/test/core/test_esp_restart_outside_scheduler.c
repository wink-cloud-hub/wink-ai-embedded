/* SPDX-License-Identifier: GPL-3.0-only */
#include "esp_system.h"

int main(void) {
    esp_restart();
    return 0; /* Reaching this line violates the public noreturn contract. */
}
