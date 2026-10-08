// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_vfs_ram.h
 * @brief Pure RAM Inode Tree Sandbox VFS for ESP-IDF Simulation (ADR-0092 Tier 2).
 */
#ifndef ESP_VFS_RAM_H
#define ESP_VFS_RAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_VFS_RAM_MAX_INODES 64u
#define ESP_VFS_RAM_MAX_FDS    16u
#define ESP_VFS_RAM_NAME_MAX   32u

typedef struct {
    char    name[ESP_VFS_RAM_NAME_MAX];
    bool    in_use;
    bool    is_dir;
    int     parent_idx;
    uint8_t *data;
    size_t  size;
    size_t  capacity;
} esp_vfs_ram_inode_t;

/** Initialize RAM VFS root */
void esp_vfs_ram_init(void);

/** Reset/Format entire RAM VFS tree (clears all inodes, frees memory) */
void esp_vfs_ram_reset(void);

/** File / Dir operations (pure RAM sandbox) */
int     esp_vfs_ram_open(const char *path, int flags, int mode);
int     esp_vfs_ram_close(int fd);
ssize_t esp_vfs_ram_read(int fd, void *dst, size_t size);
ssize_t esp_vfs_ram_write(int fd, const void *src, size_t size);
off_t   esp_vfs_ram_lseek(int fd, off_t offset, int whence);
int     esp_vfs_ram_mkdir(const char *path, mode_t mode);
int     esp_vfs_ram_unlink(const char *path);
int     esp_vfs_ram_stat(const char *path, struct stat *st);
size_t  esp_vfs_ram_get_used_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_VFS_RAM_H */
