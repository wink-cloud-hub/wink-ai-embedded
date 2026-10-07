/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif

#ifdef __cplusplus
extern "C" {
#endif

char *inet_ntoa_r(const struct in_addr addr, char *buf, int buflen);
char *inet6_ntoa_r(const struct in6_addr addr, char *buf, int buflen);

#ifdef __cplusplus
}
#endif
