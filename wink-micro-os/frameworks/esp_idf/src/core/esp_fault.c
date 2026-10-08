// SPDX-License-Identifier: LGPL-3.0-only
#include "esp_sim_fault.h"
#include <string.h>

#define ESP_FAULT_MAX_ACTIVE 16

typedef struct {
    uint32_t domain;
    uint32_t fault_type;
    uint32_t param;
    bool in_use;
} esp_fault_record_t;

static esp_fault_record_t s_active_faults[ESP_FAULT_MAX_ACTIVE];

int sim_esp_fault_inject(uint32_t domain, uint32_t fault_type, uint32_t param) {
    if (domain == ESP_FAULT_DOMAIN_NONE || domain >= ESP_FAULT_DOMAIN_MAX) {
        return -1;
    }
    if (fault_type == ESP_FAULT_TYPE_NONE || fault_type >= ESP_FAULT_TYPE_MAX) {
        return -1;
    }

    /* Check if already registered: update param */
    for (int i = 0; i < ESP_FAULT_MAX_ACTIVE; i++) {
        if (s_active_faults[i].in_use &&
            s_active_faults[i].domain == domain &&
            s_active_faults[i].fault_type == fault_type) {
            s_active_faults[i].param = param;
            return 0;
        }
    }

    /* Find free slot */
    for (int i = 0; i < ESP_FAULT_MAX_ACTIVE; i++) {
        if (!s_active_faults[i].in_use) {
            s_active_faults[i].in_use = true;
            s_active_faults[i].domain = domain;
            s_active_faults[i].fault_type = fault_type;
            s_active_faults[i].param = param;
            if (domain == ESP_FAULT_DOMAIN_TASK) {
                if (fault_type == ESP_FAULT_TASK_SUSPEND) {
                    sim_esp_task_suspend(param == 0 ? "task" : NULL);
                } else if (fault_type == ESP_FAULT_TASK_RESUME) {
                    sim_esp_task_resume(param == 0 ? "task" : NULL);
                }
            }
            return 0;
        }
    }

    return -1; /* Table full */
}

int sim_esp_fault_clear(void) {
    sim_esp_task_resume("task");
    memset(s_active_faults, 0, sizeof(s_active_faults));
    return 0;
}

bool sim_esp_fault_is_active(uint32_t domain, uint32_t fault_type) {
    for (int i = 0; i < ESP_FAULT_MAX_ACTIVE; i++) {
        if (s_active_faults[i].in_use &&
            s_active_faults[i].domain == domain &&
            s_active_faults[i].fault_type == fault_type) {
            return true;
        }
    }
    return false;
}

uint32_t sim_esp_fault_get_param(uint32_t domain, uint32_t fault_type) {
    for (int i = 0; i < ESP_FAULT_MAX_ACTIVE; i++) {
        if (s_active_faults[i].in_use &&
            s_active_faults[i].domain == domain &&
            s_active_faults[i].fault_type == fault_type) {
            return s_active_faults[i].param;
        }
    }
    return 0;
}

void esp_fault_sim_reset(void) {
    sim_esp_fault_clear();
}
