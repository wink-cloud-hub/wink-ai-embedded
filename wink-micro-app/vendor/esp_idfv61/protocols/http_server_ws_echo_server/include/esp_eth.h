/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(ETH_EVENT);
enum {
    ETHERNET_EVENT_DISCONNECTED = 0
};
