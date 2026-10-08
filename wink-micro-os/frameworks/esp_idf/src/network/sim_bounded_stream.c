/* SPDX-License-Identifier: LGPL-3.0-only */
#include "sim_bounded_stream.h"
#include <string.h>

void sim_bounded_stream_init(sim_bounded_stream_t *stream) {
    if (!stream) {
        return;
    }
    memset(stream, 0, sizeof(*stream));
}

void sim_bounded_stream_reset(sim_bounded_stream_t *stream) {
    if (!stream) {
        return;
    }
    memset(stream, 0, sizeof(*stream));
}

static sim_stream_block_t *allocate_block(sim_bounded_stream_t *stream) {
    for (size_t i = 0; i < SIM_STREAM_MAX_BLOCKS; i++) {
        if (!stream->used[i]) {
            stream->used[i] = true;
            stream->current_allocated_blocks++;
            if (stream->current_allocated_blocks > stream->peak_allocated_blocks) {
                stream->peak_allocated_blocks = stream->current_allocated_blocks;
            }
            sim_stream_block_t *blk = &stream->pool[i];
            memset(blk, 0, sizeof(*blk));
            return blk;
        }
    }
    stream->backpressure_triggered = true;
    return NULL;
}

static void release_block(sim_bounded_stream_t *stream, sim_stream_block_t *blk) {
    for (size_t i = 0; i < SIM_STREAM_MAX_BLOCKS; i++) {
        if (&stream->pool[i] == blk) {
            stream->used[i] = false;
            if (stream->current_allocated_blocks > 0) {
                stream->current_allocated_blocks--;
            }
            break;
        }
    }
}

static size_t sim_stream_get_writable_capacity(const sim_bounded_stream_t *stream) {
    size_t tail_space = 0;
    if (stream->tail && stream->tail->write_len < SIM_STREAM_BLOCK_SIZE) {
        tail_space = SIM_STREAM_BLOCK_SIZE - stream->tail->write_len;
    }
    size_t free_blocks = 0;
    for (size_t i = 0; i < SIM_STREAM_MAX_BLOCKS; i++) {
        if (!stream->used[i]) {
            free_blocks++;
        }
    }
    return tail_space + (free_blocks * SIM_STREAM_BLOCK_SIZE);
}

esp_err_t sim_bounded_stream_write(sim_bounded_stream_t *stream, const uint8_t *data, size_t len) {
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t capacity = sim_stream_get_writable_capacity(stream);
    if (len > capacity) {
        stream->backpressure_triggered = true;
        return ESP_ERR_NO_MEM;
    }

    size_t remaining = len;
    const uint8_t *src = data;

    while (remaining > 0) {
        sim_stream_block_t *blk = stream->tail;
        if (!blk || blk->write_len >= SIM_STREAM_BLOCK_SIZE) {
            blk = allocate_block(stream);
            if (!blk) {
                return ESP_ERR_NO_MEM;
            }
            if (!stream->head) {
                stream->head = blk;
            } else {
                stream->tail->next = blk;
            }
            stream->tail = blk;
        }

        size_t space = SIM_STREAM_BLOCK_SIZE - blk->write_len;
        size_t to_write = (remaining < space) ? remaining : space;
        memcpy(blk->data + blk->write_len, src, to_write);
        blk->write_len += to_write;
        src += to_write;
        remaining -= to_write;
    }

    return ESP_OK;
}

int sim_bounded_stream_read(sim_bounded_stream_t *stream, uint8_t *buf, size_t max_len) {
    if (!stream || !buf || max_len == 0) {
        return 0;
    }

    size_t total_read = 0;
    uint8_t *dst = buf;

    while (total_read < max_len && stream->head) {
        sim_stream_block_t *blk = stream->head;
        size_t available = blk->write_len - blk->read_offset;
        size_t need = max_len - total_read;
        size_t to_read = (need < available) ? need : available;

        memcpy(dst, blk->data + blk->read_offset, to_read);
        blk->read_offset += to_read;
        dst += to_read;
        total_read += to_read;

        if (blk->read_offset >= blk->write_len) {
            stream->head = blk->next;
            if (!stream->head) {
                stream->tail = NULL;
            }
            release_block(stream, blk);
        }
    }

    return (int)total_read;
}

size_t sim_bounded_stream_available(const sim_bounded_stream_t *stream) {
    if (!stream) {
        return 0;
    }
    size_t avail = 0;
    const sim_stream_block_t *blk = stream->head;
    while (blk) {
        avail += (blk->write_len - blk->read_offset);
        blk = blk->next;
    }
    return avail;
}

size_t sim_bounded_stream_peak_blocks(const sim_bounded_stream_t *stream) {
    return stream ? stream->peak_allocated_blocks : 0;
}

bool sim_bounded_stream_is_backpressured(const sim_bounded_stream_t *stream) {
    return stream ? stream->backpressure_triggered : false;
}
