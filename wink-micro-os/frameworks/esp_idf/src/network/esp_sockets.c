/* SPDX-License-Identifier: LGPL-3.0-only */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/pal_uart.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WINK_SIM_EXPORT
#endif

#define TAG "esp_sockets"

#define SIM_SOCK_BASE       100
#define MAX_SIM_SOCKETS     8
#define SIM_RX_BUF_SIZE     1024

typedef struct {
    bool in_use;
    int domain;
    int type;
    int protocol;
    bool connected;
    bool bound;
    bool listening;
    bool is_server_side;
    int reuseaddr;
    int keepalive;
    struct timeval rcvtimeo;
    struct timeval sndtimeo;
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    uint8_t rx_buf[SIM_RX_BUF_SIZE];
    size_t rx_len;
    size_t rx_read_pos;
    uint32_t echo_count;
} sim_socket_t;

static sim_socket_t s_sim_sockets[MAX_SIM_SOCKETS];
static int s_sim_connect_count = 0;
static size_t s_sim_total_tx_bytes = 0;
static size_t s_sim_total_rx_bytes = 0;
static int s_sim_fault_mode = 0; /* 0: normal, 1: fail connect, 2: fail send, 3: fail recv */

static sim_socket_t *get_sim_socket(int fd) {
    int idx = fd - SIM_SOCK_BASE;
    if (idx < 0 || idx >= MAX_SIM_SOCKETS) {
        return NULL;
    }
    if (!s_sim_sockets[idx].in_use) {
        return NULL;
    }
    return &s_sim_sockets[idx];
}

WINK_SIM_EXPORT void sim_sockets_reset(void) {
    memset(s_sim_sockets, 0, sizeof(s_sim_sockets));
    s_sim_connect_count = 0;
    s_sim_total_tx_bytes = 0;
    s_sim_total_rx_bytes = 0;
    s_sim_fault_mode = 0;
}

WINK_SIM_EXPORT int sim_sockets_get_connect_count(void) {
    return s_sim_connect_count;
}

WINK_SIM_EXPORT int sim_sockets_get_tx_bytes(void) {
    return (int)s_sim_total_tx_bytes;
}

WINK_SIM_EXPORT int sim_sockets_get_rx_bytes(void) {
    return (int)s_sim_total_rx_bytes;
}

WINK_SIM_EXPORT void sim_sockets_set_fault(int fault) {
    s_sim_fault_mode = fault;
}

#if defined(__EMSCRIPTEN__)
int socket(int domain, int type, int protocol) {
    for (int i = 0; i < MAX_SIM_SOCKETS; i++) {
        if (!s_sim_sockets[i].in_use) {
            memset(&s_sim_sockets[i], 0, sizeof(sim_socket_t));
            s_sim_sockets[i].in_use = true;
            s_sim_sockets[i].domain = domain;
            s_sim_sockets[i].type = type;
            s_sim_sockets[i].protocol = protocol;
            return SIM_SOCK_BASE + i;
        }
    }
    errno = EMFILE;
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    if (s_sim_fault_mode == 1) {
        errno = ECONNREFUSED;
        return -1;
    }
    if (addr && addrlen > 0) {
        size_t cp_len = addrlen < sizeof(s->remote_addr) ? addrlen : sizeof(s->remote_addr);
        memcpy(&s->remote_addr, addr, cp_len);
    }
    /* Advance virtual simulation time so fiber scheduler can tick */
    vTaskDelay(pdMS_TO_TICKS(10));
    s->connected = true;
    s->echo_count = 0;
    s_sim_connect_count++;
    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    if (addr && addrlen > 0) {
        size_t cp_len = addrlen < sizeof(s->local_addr) ? addrlen : sizeof(s->local_addr);
        memcpy(&s->local_addr, addr, cp_len);
    }
    s->bound = true;
    return 0;
}

int listen(int sockfd, int backlog) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    s->listening = true;
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s || !s->listening) {
        errno = EINVAL;
        return -1;
    }
    if (s_sim_fault_mode == 1) {
        errno = ECONNABORTED;
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    int client_fd = socket(s->domain, s->type, s->protocol);
    if (client_fd >= 0) {
        sim_socket_t *cs = get_sim_socket(client_fd);
        if (cs) {
            cs->connected = true;
            cs->is_server_side = true;
            /* In simulated server mode, client sends initial request payload */
            const char *client_msg = "Data to ESP";
            size_t msg_len = strlen(client_msg);
            memcpy(cs->rx_buf, client_msg, msg_len);
            cs->rx_len = msg_len;
            cs->rx_read_pos = 0;
            s_sim_connect_count++;
        }
    }
    if (addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        memset(sin, 0, sizeof(struct sockaddr_in));
        sin->sin_family = AF_INET;
        sin->sin_port = htons(54321);
        inet_pton(AF_INET, "127.0.0.1", &sin->sin_addr);
        *addrlen = sizeof(struct sockaddr_in);
    }
    return client_fd;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s || !s->connected) {
        errno = ENOTCONN;
        return -1;
    }
    if (s_sim_fault_mode == 2) {
        errno = EIO;
        return -1;
    }
    /* Yield to FreeRTOS scheduler to advance simulation time */
    vTaskDelay(pdMS_TO_TICKS(20));
    s_sim_total_tx_bytes += len;

    if (!s->is_server_side) {
        /* Prepare simulated echo server response for outbound client */
        size_t copy_len = len < SIM_RX_BUF_SIZE ? len : SIM_RX_BUF_SIZE;
        memcpy(s->rx_buf, buf, copy_len);
        s->rx_len = copy_len;
        s->rx_read_pos = 0;
    }
    s->echo_count++;

    return (ssize_t)len;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s || !s->connected) {
        errno = ENOTCONN;
        return -1;
    }
    if (s_sim_fault_mode == 3) {
        errno = ECONNRESET;
        return -1;
    }
    /* Yield to FreeRTOS scheduler to advance simulation time */
    vTaskDelay(pdMS_TO_TICKS(20));

    if (s->is_server_side) {
        if (s->rx_read_pos >= s->rx_len) {
            /* Client received echoed response and closed connection */
            return 0;
        }
    } else {
        /* Outbound client: After 3 echo packets, simulate remote server disconnect to exercise recovery */
        if (s->echo_count > 3) {
            s->connected = false;
            errno = ECONNRESET;
            return -1;
        }

        if (s->rx_read_pos >= s->rx_len) {
            /* No data available, return 0 for EOF */
            return 0;
        }
    }

    size_t avail = s->rx_len - s->rx_read_pos;
    size_t to_read = len < avail ? len : avail;
    memcpy(buf, s->rx_buf + s->rx_read_pos, to_read);
    s->rx_read_pos += to_read;
    s_sim_total_rx_bytes += to_read;

    return (ssize_t)to_read;
}

char *inet_ntoa_r(const struct in_addr addr, char *buf, int buflen) {
    if (!buf || buflen <= 0) {
        return NULL;
    }
    return (char *)inet_ntop(AF_INET, &addr, buf, (socklen_t)buflen);
}

char *inet6_ntoa_r(const struct in6_addr addr, char *buf, int buflen) {
    if (!buf || buflen <= 0) {
        return NULL;
    }
    return (char *)inet_ntop(AF_INET6, &addr, buf, (socklen_t)buflen);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    return send(sockfd, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    return recv(sockfd, buf, len, flags);
}

int shutdown(int sockfd, int how) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    s->connected = false;
    return 0;
}

int closesocket(int sockfd) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    s->in_use = false;
    s->connected = false;
    s->bound = false;
    s->listening = false;
    return 0;
}

int close(int fd) {
    sim_socket_t *s = get_sim_socket(fd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    s->in_use = false;
    s->connected = false;
    s->bound = false;
    s->listening = false;
    return 0;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    if (!optval) {
        errno = EFAULT;
        return -1;
    }
    if (level == SOL_SOCKET) {
        if (optname == SO_REUSEADDR && optlen >= sizeof(int)) {
            s->reuseaddr = *(const int *)optval;
            return 0;
        }
        if (optname == SO_KEEPALIVE && optlen >= sizeof(int)) {
            s->keepalive = *(const int *)optval;
            return 0;
        }
        if (optname == SO_RCVTIMEO && optlen >= sizeof(struct timeval)) {
            s->rcvtimeo = *(const struct timeval *)optval;
            return 0;
        }
        if (optname == SO_SNDTIMEO && optlen >= sizeof(struct timeval)) {
            s->sndtimeo = *(const struct timeval *)optval;
            return 0;
        }
    }
    return 0;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    sim_socket_t *s = get_sim_socket(sockfd);
    if (!s) {
        errno = EBADF;
        return -1;
    }
    if (!optval || !optlen) {
        errno = EFAULT;
        return -1;
    }
    if (level == SOL_SOCKET) {
        if (optname == SO_REUSEADDR && *optlen >= sizeof(int)) {
            *(int *)optval = s->reuseaddr;
            *optlen = sizeof(int);
            return 0;
        }
        if (optname == SO_KEEPALIVE && *optlen >= sizeof(int)) {
            *(int *)optval = s->keepalive;
            *optlen = sizeof(int);
            return 0;
        }
        if (optname == SO_RCVTIMEO && *optlen >= sizeof(struct timeval)) {
            *(struct timeval *)optval = s->rcvtimeo;
            *optlen = sizeof(struct timeval);
            return 0;
        }
        if (optname == SO_SNDTIMEO && *optlen >= sizeof(struct timeval)) {
            *(struct timeval *)optval = s->sndtimeo;
            *optlen = sizeof(struct timeval);
            return 0;
        }
    }
    return 0;
}
#endif
