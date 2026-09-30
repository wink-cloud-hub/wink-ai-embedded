/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef SIM_BOUNDED_STREAM_H_
#define SIM_BOUNDED_STREAM_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_STREAM_BLOCK_SIZE   1024
#define SIM_STREAM_MAX_BLOCKS   4   /* 硬性约束：最多 4 块，即 4096 字节常驻内存 */

typedef struct sim_stream_block {
    uint8_t data[SIM_STREAM_BLOCK_SIZE];
    size_t write_len;
    size_t read_offset;
    struct sim_stream_block *next;
} sim_stream_block_t;

typedef struct {
    sim_stream_block_t pool[SIM_STREAM_MAX_BLOCKS];
    bool used[SIM_STREAM_MAX_BLOCKS];
    sim_stream_block_t *head;
    sim_stream_block_t *tail;
    size_t current_allocated_blocks;
    size_t peak_allocated_blocks;
    bool backpressure_triggered;
} sim_bounded_stream_t;

void sim_bounded_stream_init(sim_bounded_stream_t *stream);
void sim_bounded_stream_reset(sim_bounded_stream_t *stream);
esp_err_t sim_bounded_stream_write(sim_bounded_stream_t *stream, const uint8_t *data, size_t len);
int sim_bounded_stream_read(sim_bounded_stream_t *stream, uint8_t *buf, size_t max_len);
size_t sim_bounded_stream_available(const sim_bounded_stream_t *stream);
size_t sim_bounded_stream_peak_blocks(const sim_bounded_stream_t *stream);
bool sim_bounded_stream_is_backpressured(const sim_bounded_stream_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* SIM_BOUNDED_STREAM_H_ */
