// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_partition_sim.h
 * @brief Simulation Partition Table and In-Memory Flash Model (ADR-0092 Tier 2).
 */
#ifndef ESP_PARTITION_SIM_H
#define ESP_PARTITION_SIM_H

#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Reset simulated partition storage to initial state (frees lazy sectors, restores default layout) */
void esp_partition_sim_reset(void);

/** Get partition definition by index (0..count-1) */
const esp_partition_t *esp_partition_sim_get_by_index(size_t index);

/** Total simulated partition count */
size_t esp_partition_sim_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_PARTITION_SIM_H */
