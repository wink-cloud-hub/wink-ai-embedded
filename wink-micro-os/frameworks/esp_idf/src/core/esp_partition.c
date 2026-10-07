// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_partition.c
 * @brief Simulated In-Memory Flash Partition Table for WinkMicroOS (ADR-0092 Tier 2).
 */

#include "esp_partition.h"
#include "esp_partition_sim.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define SIM_FLASH_SECTOR_SIZE 4096u
#define SIM_PARTITION_COUNT   6u
#define SIM_MAX_ITERATORS     8u

typedef struct {
    esp_partition_t info;
    uint32_t        sector_count;
    uint8_t       **sectors;
} sim_partition_slot_t;

static sim_partition_slot_t s_partitions[SIM_PARTITION_COUNT] = {
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_DATA,
            .subtype = ESP_PARTITION_SUBTYPE_DATA_NVS,
            .address = 0x9000u,
            .size = 0x6000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "nvs",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 6u, /* 24 KB / 4 KB */
        .sectors = NULL,
    },
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_DATA,
            .subtype = ESP_PARTITION_SUBTYPE_DATA_PHY,
            .address = 0xf000u,
            .size = 0x1000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "phy_init",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 1u, /* 4 KB */
        .sectors = NULL,
    },
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_APP,
            .subtype = ESP_PARTITION_SUBTYPE_APP_FACTORY,
            .address = 0x10000u,
            .size = 0x100000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "factory",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 256u, /* 1 MB */
        .sectors = NULL,
    },
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_APP,
            .subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0,
            .address = 0x110000u,
            .size = 0x100000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "ota_0",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 256u, /* 1 MB */
        .sectors = NULL,
    },
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_APP,
            .subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1,
            .address = 0x210000u,
            .size = 0x100000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "ota_1",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 256u, /* 1 MB */
        .sectors = NULL,
    },
    {
        .info = {
            .flash_chip = NULL,
            .type = ESP_PARTITION_TYPE_DATA,
            .subtype = ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            .address = 0x310000u,
            .size = 0xF0000u,
            .erase_size = SIM_FLASH_SECTOR_SIZE,
            .label = "storage",
            .encrypted = false,
            .readonly = false,
        },
        .sector_count = 240u, /* 960 KB */
        .sectors = NULL,
    },
};

struct esp_partition_iterator_opaque_ {
    bool                    in_use;
    esp_partition_type_t    type;
    esp_partition_subtype_t subtype;
    char                    label[17];
    bool                    has_label;
    size_t                  current_index;
};

static struct esp_partition_iterator_opaque_ s_iterators[SIM_MAX_ITERATORS];

static bool partition_matches(const esp_partition_t *part,
                              esp_partition_type_t type,
                              esp_partition_subtype_t subtype,
                              const char *label)
{
    if (type != ESP_PARTITION_TYPE_ANY && part->type != type) {
        return false;
    }
    if (subtype != ESP_PARTITION_SUBTYPE_ANY && part->subtype != subtype) {
        return false;
    }
    if (label != NULL && strcmp(part->label, label) != 0) {
        return false;
    }
    return true;
}

static sim_partition_slot_t *find_slot_by_partition(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return NULL;
    }
    for (size_t i = 0u; i < SIM_PARTITION_COUNT; i++) {
        if (&s_partitions[i].info == partition ||
            (s_partitions[i].info.address == partition->address &&
             s_partitions[i].info.size == partition->size)) {
            return &s_partitions[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ESP Partition Public APIs
 * ═══════════════════════════════════════════════════════════════════════════ */

const esp_partition_t *esp_partition_find_first(esp_partition_type_t type,
                                               esp_partition_subtype_t subtype,
                                               const char *label)
{
    for (size_t i = 0u; i < SIM_PARTITION_COUNT; i++) {
        if (partition_matches(&s_partitions[i].info, type, subtype, label)) {
            return &s_partitions[i].info;
        }
    }
    return NULL;
}

esp_err_t esp_partition_find_first_err(esp_partition_type_t type,
                                       esp_partition_subtype_t subtype,
                                       const char *label,
                                       const esp_partition_t **out_partition)
{
    if (out_partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_partition_t *p = esp_partition_find_first(type, subtype, label);
    if (p == NULL) {
        *out_partition = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    *out_partition = p;
    return ESP_OK;
}

esp_partition_iterator_t esp_partition_find(esp_partition_type_t type,
                                           esp_partition_subtype_t subtype,
                                           const char *label)
{
    for (size_t it_idx = 0u; it_idx < SIM_MAX_ITERATORS; it_idx++) {
        if (!s_iterators[it_idx].in_use) {
            s_iterators[it_idx].in_use = true;
            s_iterators[it_idx].type = type;
            s_iterators[it_idx].subtype = subtype;
            if (label != NULL) {
                strncpy(s_iterators[it_idx].label, label, sizeof(s_iterators[it_idx].label) - 1u);
                s_iterators[it_idx].label[sizeof(s_iterators[it_idx].label) - 1u] = '\0';
                s_iterators[it_idx].has_label = true;
            } else {
                s_iterators[it_idx].has_label = false;
            }

            for (size_t i = 0u; i < SIM_PARTITION_COUNT; i++) {
                if (partition_matches(&s_partitions[i].info, type, subtype, label)) {
                    s_iterators[it_idx].current_index = i;
                    return &s_iterators[it_idx];
                }
            }

            s_iterators[it_idx].in_use = false;
            return NULL;
        }
    }
    return NULL;
}

esp_err_t esp_partition_find_err(esp_partition_type_t type,
                                 esp_partition_subtype_t subtype,
                                 const char *label,
                                 esp_partition_iterator_t *out_iterator)
{
    if (out_iterator == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_iterator = esp_partition_find(type, subtype, label);
    return (*out_iterator != NULL) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

const esp_partition_t *esp_partition_get(esp_partition_iterator_t iterator)
{
    if (iterator == NULL || !iterator->in_use || iterator->current_index >= SIM_PARTITION_COUNT) {
        return NULL;
    }
    return &s_partitions[iterator->current_index].info;
}

esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator)
{
    if (iterator == NULL || !iterator->in_use) {
        return NULL;
    }

    const char *lbl = iterator->has_label ? iterator->label : NULL;
    for (size_t i = iterator->current_index + 1u; i < SIM_PARTITION_COUNT; i++) {
        if (partition_matches(&s_partitions[i].info, iterator->type, iterator->subtype, lbl)) {
            iterator->current_index = i;
            return iterator;
        }
    }

    iterator->in_use = false;
    return NULL;
}

void esp_partition_iterator_release(esp_partition_iterator_t iterator)
{
    if (iterator != NULL) {
        iterator->in_use = false;
    }
}

uint32_t esp_partition_get_main_flash_sector_size(void)
{
    return SIM_FLASH_SECTOR_SIZE;
}

esp_err_t esp_partition_read(const esp_partition_t *partition,
                             size_t src_offset, void *dst, size_t size)
{
    if (partition == NULL || dst == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (src_offset + size > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (size == 0u) {
        return ESP_OK;
    }

    sim_partition_slot_t *slot = find_slot_by_partition(partition);
    if (slot == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t *dst_ptr = (uint8_t *)dst;
    size_t remaining = size;
    size_t curr_offset = src_offset;

    while (remaining > 0u) {
        uint32_t sector_idx = (uint32_t)(curr_offset / SIM_FLASH_SECTOR_SIZE);
        uint32_t offset_in_sector = (uint32_t)(curr_offset % SIM_FLASH_SECTOR_SIZE);
        size_t chunk = SIM_FLASH_SECTOR_SIZE - offset_in_sector;
        if (chunk > remaining) {
            chunk = remaining;
        }

        if (slot->sectors != NULL && sector_idx < slot->sector_count && slot->sectors[sector_idx] != NULL) {
            memcpy(dst_ptr, slot->sectors[sector_idx] + offset_in_sector, chunk);
        } else {
            /* Unwritten flash reads as 0xFF */
            memset(dst_ptr, 0xFF, chunk);
        }

        curr_offset += chunk;
        dst_ptr += chunk;
        remaining -= chunk;
    }

    return ESP_OK;
}

esp_err_t esp_partition_read_raw(const esp_partition_t *partition,
                                 size_t src_offset, void *dst, size_t size)
{
    return esp_partition_read(partition, src_offset, dst, size);
}

esp_err_t esp_partition_write(const esp_partition_t *partition,
                              size_t dst_offset, const void *src, size_t size)
{
    if (partition == NULL || src == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (dst_offset + size > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (size == 0u) {
        return ESP_OK;
    }

    sim_partition_slot_t *slot = find_slot_by_partition(partition);
    if (slot == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Allocate sector pointer array lazily */
    if (slot->sectors == NULL) {
        slot->sectors = (uint8_t **)calloc(slot->sector_count, sizeof(uint8_t *));
        if (slot->sectors == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    const uint8_t *src_ptr = (const uint8_t *)src;
    size_t remaining = size;
    size_t curr_offset = dst_offset;

    while (remaining > 0u) {
        uint32_t sector_idx = (uint32_t)(curr_offset / SIM_FLASH_SECTOR_SIZE);
        uint32_t offset_in_sector = (uint32_t)(curr_offset % SIM_FLASH_SECTOR_SIZE);
        size_t chunk = SIM_FLASH_SECTOR_SIZE - offset_in_sector;
        if (chunk > remaining) {
            chunk = remaining;
        }

        if (slot->sectors[sector_idx] == NULL) {
            slot->sectors[sector_idx] = (uint8_t *)malloc(SIM_FLASH_SECTOR_SIZE);
            if (slot->sectors[sector_idx] == NULL) {
                return ESP_ERR_NO_MEM;
            }
            memset(slot->sectors[sector_idx], 0xFF, SIM_FLASH_SECTOR_SIZE);
        }

        memcpy(slot->sectors[sector_idx] + offset_in_sector, src_ptr, chunk);

        curr_offset += chunk;
        src_ptr += chunk;
        remaining -= chunk;
    }

    return ESP_OK;
}

esp_err_t esp_partition_write_raw(const esp_partition_t *partition,
                                  size_t dst_offset, const void *src, size_t size)
{
    return esp_partition_write(partition, dst_offset, src, size);
}

esp_err_t esp_partition_erase_range(const esp_partition_t *partition,
                                    size_t offset, size_t size)
{
    if (partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (offset % SIM_FLASH_SECTOR_SIZE != 0 || size % SIM_FLASH_SECTOR_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (offset + size > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }

    sim_partition_slot_t *slot = find_slot_by_partition(partition);
    if (slot == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    if (slot->sectors == NULL) {
        return ESP_OK;
    }

    uint32_t start_sec = (uint32_t)(offset / SIM_FLASH_SECTOR_SIZE);
    uint32_t num_secs = (uint32_t)(size / SIM_FLASH_SECTOR_SIZE);

    for (uint32_t i = 0u; i < num_secs; i++) {
        uint32_t sec = start_sec + i;
        if (sec < slot->sector_count && slot->sectors[sec] != NULL) {
            memset(slot->sectors[sec], 0xFF, SIM_FLASH_SECTOR_SIZE);
        }
    }

    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Simulation Control
 * ═══════════════════════════════════════════════════════════════════════════ */

void esp_partition_sim_reset(void)
{
    for (size_t i = 0u; i < SIM_PARTITION_COUNT; i++) {
        if (s_partitions[i].sectors != NULL) {
            for (size_t s = 0u; s < s_partitions[i].sector_count; s++) {
                if (s_partitions[i].sectors[s] != NULL) {
                    free(s_partitions[i].sectors[s]);
                    s_partitions[i].sectors[s] = NULL;
                }
            }
            free(s_partitions[i].sectors);
            s_partitions[i].sectors = NULL;
        }
    }
    for (size_t it = 0u; it < SIM_MAX_ITERATORS; it++) {
        s_iterators[it].in_use = false;
    }
}

const esp_partition_t *esp_partition_sim_get_by_index(size_t index)
{
    if (index >= SIM_PARTITION_COUNT) {
        return NULL;
    }
    return &s_partitions[index].info;
}

size_t esp_partition_sim_get_count(void)
{
    return SIM_PARTITION_COUNT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard SHA-256 for Partition Content / Identity Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sim_sha256_t;

static void sim_sha256_transform(uint32_t state[8], const uint8_t buffer[64]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    uint32_t a, b, c, d, e, f, g, h, W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = (((W[i - 15] >> 7) | (W[i - 15] << 25)) ^
                       ((W[i - 15] >> 18) | (W[i - 15] << 14)) ^
                       (W[i - 15] >> 3));
        uint32_t s1 = (((W[i - 2] >> 17) | (W[i - 2] << 15)) ^
                       ((W[i - 2] >> 19) | (W[i - 2] << 13)) ^
                       (W[i - 2] >> 10));
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sim_sha256_init(sim_sha256_t *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sim_sha256_update(sim_sha256_t *ctx, const uint8_t *data, size_t len) {
    size_t idx = (size_t)(ctx->count & 0x3f);
    ctx->count += len;
    while (len > 0) {
        size_t take = 64 - idx;
        if (take > len) take = len;
        memcpy(&ctx->buffer[idx], data, take);
        data += take;
        len -= take;
        idx += take;
        if (idx == 64) {
            sim_sha256_transform(ctx->state, ctx->buffer);
            idx = 0;
        }
    }
}

static void sim_sha256_final(sim_sha256_t *ctx, uint8_t hash[32]) {
    uint64_t total_bits = ctx->count * 8;
    size_t idx = (size_t)(ctx->count & 0x3f);
    ctx->buffer[idx++] = 0x80;
    if (idx > 56) {
        memset(&ctx->buffer[idx], 0, 64 - idx);
        sim_sha256_transform(ctx->state, ctx->buffer);
        idx = 0;
    }
    memset(&ctx->buffer[idx], 0, 56 - idx);
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[56 + (7 - i)] = (uint8_t)(total_bits >> (i * 8));
    }
    sim_sha256_transform(ctx->state, ctx->buffer);
    for (int i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

esp_err_t esp_partition_get_sha256(const esp_partition_t* partition, uint8_t* sha_256)
{
    if (partition == NULL || sha_256 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_sha256_t ctx;
    sim_sha256_init(&ctx);
    sim_sha256_update(&ctx, (const uint8_t*)&partition->type, sizeof(partition->type));
    sim_sha256_update(&ctx, (const uint8_t*)&partition->subtype, sizeof(partition->subtype));
    sim_sha256_update(&ctx, (const uint8_t*)&partition->address, sizeof(partition->address));
    sim_sha256_update(&ctx, (const uint8_t*)&partition->size, sizeof(partition->size));
    if (partition->label[0] != '\0') {
        sim_sha256_update(&ctx, (const uint8_t*)partition->label, strlen(partition->label));
    }

    sim_partition_slot_t *slot = find_slot_by_partition(partition);
    if (slot && slot->sectors) {
        for (uint32_t s = 0; s < slot->sector_count; s++) {
            if (slot->sectors[s]) {
                sim_sha256_update(&ctx, slot->sectors[s], SIM_FLASH_SECTOR_SIZE);
            }
        }
    }
    sim_sha256_final(&ctx, sha_256);
    return ESP_OK;
}

