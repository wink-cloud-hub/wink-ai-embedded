// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_vfs_ram.c
 * @brief Pure RAM Inode Tree Sandbox VFS for ESP-IDF Simulation (ADR-0092 Tier 2).
 */

#include "esp_vfs_ram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifndef O_ACCMODE
#  define O_ACCMODE 0x0003
#endif

typedef struct {
    bool   in_use;
    int    inode_idx;
    size_t offset;
    int    flags;
} sim_fd_entry_t;

static esp_vfs_ram_inode_t s_inodes[ESP_VFS_RAM_MAX_INODES];
static sim_fd_entry_t      s_fds[ESP_VFS_RAM_MAX_FDS];
static bool                s_vfs_inited = false;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Path and Inode Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static int find_free_inode(void)
{
    for (size_t i = 1u; i < ESP_VFS_RAM_MAX_INODES; i++) {
        if (!s_inodes[i].in_use) {
            return (int)i;
        }
    }
    return -1;
}

static int find_child_inode(int parent_idx, const char *name)
{
    for (size_t i = 0u; i < ESP_VFS_RAM_MAX_INODES; i++) {
        if (s_inodes[i].in_use &&
            s_inodes[i].parent_idx == parent_idx &&
            strcmp(s_inodes[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Traverse a path string and find target inode or parent directory.
 * @param path Input path (e.g. "/spiffs/data.txt")
 * @param out_parent_idx Receives parent inode index
 * @param out_leaf Receives leaf filename buffer (at least ESP_VFS_RAM_NAME_MAX bytes)
 * @return Target inode index, or -1 if target leaf does not exist.
 */
static int resolve_path(const char *path, int *out_parent_idx, char *out_leaf)
{
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (!s_vfs_inited) {
        esp_vfs_ram_init();
    }

    int current_dir = 0; /* root */
    const char *p = path;
    while (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        /* Path is root "/" */
        if (out_parent_idx) { *out_parent_idx = -1; }
        if (out_leaf) { out_leaf[0] = '\0'; }
        return 0;
    }

    char token[ESP_VFS_RAM_NAME_MAX];
    while (*p != '\0') {
        size_t len = 0;
        while (p[len] != '/' && p[len] != '\0' && len + 1 < sizeof(token)) {
            token[len] = p[len];
            len++;
        }
        token[len] = '\0';
        p += len;
        while (*p == '/') {
            p++;
        }

        if (*p == '\0') {
            /* This token is the final leaf */
            if (out_parent_idx) { *out_parent_idx = current_dir; }
            if (out_leaf) {
                strncpy(out_leaf, token, ESP_VFS_RAM_NAME_MAX - 1);
                out_leaf[ESP_VFS_RAM_NAME_MAX - 1] = '\0';
            }
            return find_child_inode(current_dir, token);
        } else {
            /* Intermediate directory */
            int next = find_child_inode(current_dir, token);
            if (next < 0 || !s_inodes[next].is_dir) {
                /* Intermediate directory does not exist */
                return -1;
            }
            current_dir = next;
        }
    }

    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public VFS APIs
 * ═══════════════════════════════════════════════════════════════════════════ */

void esp_vfs_ram_init(void)
{
    if (s_vfs_inited) {
        return;
    }
    memset(s_inodes, 0, sizeof(s_inodes));
    memset(s_fds, 0, sizeof(s_fds));

    /* Root directory inode 0 */
    s_inodes[0].in_use = true;
    s_inodes[0].is_dir = true;
    s_inodes[0].parent_idx = -1;
    s_inodes[0].name[0] = '\0';

    s_vfs_inited = true;
}

void esp_vfs_ram_reset(void)
{
    for (size_t i = 0u; i < ESP_VFS_RAM_MAX_INODES; i++) {
        if (s_inodes[i].in_use && s_inodes[i].data != NULL) {
            free(s_inodes[i].data);
            s_inodes[i].data = NULL;
        }
        s_inodes[i].in_use = false;
    }
    memset(s_fds, 0, sizeof(s_fds));
    s_vfs_inited = false;
    esp_vfs_ram_init();
}

int esp_vfs_ram_open(const char *path, int flags, int mode)
{
    (void)mode;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    int parent_idx = -1;
    char leaf[ESP_VFS_RAM_NAME_MAX];
    int target = resolve_path(path, &parent_idx, leaf);

    if (target >= 0) {
        /* File exists */
        if (s_inodes[target].is_dir) {
            errno = EISDIR;
            return -1;
        }
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            errno = EEXIST;
            return -1;
        }
        if (flags & O_TRUNC) {
            if (s_inodes[target].data != NULL) {
                free(s_inodes[target].data);
                s_inodes[target].data = NULL;
            }
            s_inodes[target].size = 0u;
            s_inodes[target].capacity = 0u;
        }
    } else {
        /* File does not exist */
        if (!(flags & O_CREAT)) {
            errno = ENOENT;
            return -1;
        }
        if (parent_idx < 0 || !s_inodes[parent_idx].is_dir) {
            errno = ENOENT;
            return -1;
        }

        int new_idx = find_free_inode();
        if (new_idx < 0) {
            errno = ENOSPC;
            return -1;
        }

        s_inodes[new_idx].in_use = true;
        s_inodes[new_idx].is_dir = false;
        s_inodes[new_idx].parent_idx = parent_idx;
        strncpy(s_inodes[new_idx].name, leaf, ESP_VFS_RAM_NAME_MAX - 1);
        s_inodes[new_idx].name[ESP_VFS_RAM_NAME_MAX - 1] = '\0';
        s_inodes[new_idx].data = NULL;
        s_inodes[new_idx].size = 0u;
        s_inodes[new_idx].capacity = 0u;
        target = new_idx;
    }

    /* Allocate file descriptor */
    for (size_t fd = 0u; fd < ESP_VFS_RAM_MAX_FDS; fd++) {
        if (!s_fds[fd].in_use) {
            s_fds[fd].in_use = true;
            s_fds[fd].inode_idx = target;
            s_fds[fd].flags = flags;
            s_fds[fd].offset = (flags & O_APPEND) ? s_inodes[target].size : 0u;
            return (int)(fd + 3u); /* Base offset 3 */
        }
    }

    errno = ENFILE;
    return -1;
}

int esp_vfs_ram_close(int fd)
{
    int idx = fd - 3;
    if (idx < 0 || (size_t)idx >= ESP_VFS_RAM_MAX_FDS || !s_fds[idx].in_use) {
        errno = EBADF;
        return -1;
    }
    s_fds[idx].in_use = false;
    return 0;
}

ssize_t esp_vfs_ram_read(int fd, void *dst, size_t size)
{
    int idx = fd - 3;
    if (idx < 0 || (size_t)idx >= ESP_VFS_RAM_MAX_FDS || !s_fds[idx].in_use || dst == NULL) {
        errno = EBADF;
        return -1;
    }

    esp_vfs_ram_inode_t *inode = &s_inodes[s_fds[idx].inode_idx];
    if (s_fds[idx].offset >= inode->size) {
        return 0; /* EOF */
    }

    size_t avail = inode->size - s_fds[idx].offset;
    size_t chunk = (size < avail) ? size : avail;
    if (chunk > 0u && inode->data != NULL) {
        memcpy(dst, inode->data + s_fds[idx].offset, chunk);
        s_fds[idx].offset += chunk;
    }
    return (ssize_t)chunk;
}

ssize_t esp_vfs_ram_write(int fd, const void *src, size_t size)
{
    int idx = fd - 3;
    if (idx < 0 || (size_t)idx >= ESP_VFS_RAM_MAX_FDS || !s_fds[idx].in_use || src == NULL) {
        errno = EBADF;
        return -1;
    }

    esp_vfs_ram_inode_t *inode = &s_inodes[s_fds[idx].inode_idx];
    size_t needed = s_fds[idx].offset + size;

    if (needed > inode->capacity) {
        size_t new_cap = (inode->capacity == 0u) ? 64u : (inode->capacity * 2u);
        while (new_cap < needed) {
            new_cap *= 2u;
        }
        uint8_t *new_data = (uint8_t *)realloc(inode->data, new_cap);
        if (new_data == NULL) {
            errno = ENOMEM;
            return -1;
        }
        inode->data = new_data;
        inode->capacity = new_cap;
    }

    memcpy(inode->data + s_fds[idx].offset, src, size);
    s_fds[idx].offset += size;
    if (s_fds[idx].offset > inode->size) {
        inode->size = s_fds[idx].offset;
    }

    return (ssize_t)size;
}

off_t esp_vfs_ram_lseek(int fd, off_t offset, int whence)
{
    int idx = fd - 3;
    if (idx < 0 || (size_t)idx >= ESP_VFS_RAM_MAX_FDS || !s_fds[idx].in_use) {
        errno = EBADF;
        return (off_t)-1;
    }

    esp_vfs_ram_inode_t *inode = &s_inodes[s_fds[idx].inode_idx];
    off_t target = 0;

    switch (whence) {
    case SEEK_SET:
        target = offset;
        break;
    case SEEK_CUR:
        target = (off_t)s_fds[idx].offset + offset;
        break;
    case SEEK_END:
        target = (off_t)inode->size + offset;
        break;
    default:
        errno = EINVAL;
        return (off_t)-1;
    }

    if (target < 0) {
        errno = EINVAL;
        return (off_t)-1;
    }

    s_fds[idx].offset = (size_t)target;
    return target;
}

int esp_vfs_ram_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    int parent_idx = -1;
    char leaf[ESP_VFS_RAM_NAME_MAX];
    int target = resolve_path(path, &parent_idx, leaf);

    if (target >= 0) {
        errno = EEXIST;
        return -1;
    }
    if (parent_idx < 0 || !s_inodes[parent_idx].is_dir) {
        errno = ENOENT;
        return -1;
    }

    int new_idx = find_free_inode();
    if (new_idx < 0) {
        errno = ENOSPC;
        return -1;
    }

    s_inodes[new_idx].in_use = true;
    s_inodes[new_idx].is_dir = true;
    s_inodes[new_idx].parent_idx = parent_idx;
    strncpy(s_inodes[new_idx].name, leaf, ESP_VFS_RAM_NAME_MAX - 1);
    s_inodes[new_idx].name[ESP_VFS_RAM_NAME_MAX - 1] = '\0';
    s_inodes[new_idx].data = NULL;
    s_inodes[new_idx].size = 0u;
    s_inodes[new_idx].capacity = 0u;

    return 0;
}

int esp_vfs_ram_unlink(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    int parent_idx = -1;
    char leaf[ESP_VFS_RAM_NAME_MAX];
    int target = resolve_path(path, &parent_idx, leaf);

    if (target < 0) {
        errno = ENOENT;
        return -1;
    }
    if (s_inodes[target].is_dir) {
        errno = EISDIR;
        return -1;
    }

    if (s_inodes[target].data != NULL) {
        free(s_inodes[target].data);
        s_inodes[target].data = NULL;
    }
    s_inodes[target].in_use = false;
    return 0;
}

int esp_vfs_ram_stat(const char *path, struct stat *st)
{
    if (path == NULL || st == NULL) {
        errno = EINVAL;
        return -1;
    }

    int parent_idx = -1;
    char leaf[ESP_VFS_RAM_NAME_MAX];
    int target = resolve_path(path, &parent_idx, leaf);

    if (target < 0) {
        errno = ENOENT;
        return -1;
    }

    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)s_inodes[target].size;
    st->st_mode = s_inodes[target].is_dir ? S_IFDIR : S_IFREG;
    return 0;
}

size_t esp_vfs_ram_get_used_bytes(void)
{
    size_t total_used = 0;
    for (size_t i = 0u; i < ESP_VFS_RAM_MAX_INODES; i++) {
        if (s_inodes[i].in_use && !s_inodes[i].is_dir) {
            total_used += s_inodes[i].size;
        }
    }
    return total_used;
}
