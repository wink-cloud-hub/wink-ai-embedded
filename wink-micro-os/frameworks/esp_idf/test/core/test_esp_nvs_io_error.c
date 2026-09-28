/* SPDX-License-Identifier: GPL-3.0-only */
#include "nvs_flash.h"
#include "nvs.h"

int main(void) {
    if (nvs_flash_init() != ESP_OK) return 1;
    nvs_handle_t handle = 0;
    if (nvs_open("io_failure", NVS_READWRITE, &handle) != ESP_OK) return 2;
    if (nvs_set_u32(handle, "key", 1) != ESP_OK) return 3;
    return nvs_commit(handle) == ESP_FAIL ? 0 : 4;
}
