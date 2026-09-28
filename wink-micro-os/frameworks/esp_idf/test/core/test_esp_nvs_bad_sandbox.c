/* SPDX-License-Identifier: GPL-3.0-only */
#include "nvs_flash.h"

int main(void) {
    return nvs_flash_init() == ESP_OK ? 1 : 0;
}
