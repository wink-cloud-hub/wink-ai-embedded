// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_log_host.c
 * @brief Host target tiered logging backend implementation.
 */
#include "pal_log.h"
#include "pal_osal.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <io.h>
#  include <process.h>
   typedef DWORD pal_log_tid_t;
   static inline pal_log_tid_t pal_log_gettid(void) { return GetCurrentThreadId(); }
#  define PAL_LOG_TID_FMT "0x%04lX"
#else
#  include <pthread.h>
#  include <unistd.h>
#  include <sys/time.h>
   typedef unsigned long pal_log_tid_t;
   static inline pal_log_tid_t pal_log_gettid(void) {
       return (pal_log_tid_t)(uintptr_t)pthread_self();
   }
#  define PAL_LOG_TID_FMT "0x%08lX"
#endif

#if defined(_MSC_VER)
#  define PAL_LOG_HAVE_LOCALTIME_S 1
#elif defined(__GLIBC__) || defined(__APPLE__) || defined(__linux__) || \
      (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1)
#  define PAL_LOG_HAVE_LOCALTIME_R 1
#else
#  define PAL_LOG_HAVE_LOCALTIME_PLAIN 1
#endif

#define ANSI_RESET   "\033[0m"
#define ANSI_RED_BG  "\033[41;37m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_DIM     "\033[2m"

#define PAL_LOG_LINE_BUF 768

static const char *level_letter(pal_log_level_t level)
{
    switch (level) {
    case PAL_LOG_ERROR: return "E";
    case PAL_LOG_WARN:  return "W";
    case PAL_LOG_INFO:  return "I";
    case PAL_LOG_DEBUG: return "D";
    default:            return "?";
    }
}

static const char *level_color(pal_log_level_t level)
{
    switch (level) {
    case PAL_LOG_ERROR: return ANSI_RED_BG;
    case PAL_LOG_WARN:  return ANSI_YELLOW;
    case PAL_LOG_INFO:  return ANSI_GREEN;
    case PAL_LOG_DEBUG: return ANSI_DIM;
    default:            return ANSI_RESET;
    }
}

static void pal_log_now_str(char *buf, size_t bufsz)
{
    uint64_t ms;
    struct tm tm_local;

#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uint64_t epoch_100ns = uli.QuadPart - (uint64_t)11644473600ULL * 10000000ULL;
    ms = epoch_100ns / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ms = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
#endif
    time_t sec = (time_t)(ms / 1000);
    unsigned msec = (unsigned)(ms % 1000);

#if defined(PAL_LOG_HAVE_LOCALTIME_S)
    localtime_s(&tm_local, &sec);
#elif defined(PAL_LOG_HAVE_LOCALTIME_R)
    localtime_r(&sec, &tm_local);
#elif defined(PAL_LOG_HAVE_LOCALTIME_PLAIN)
    {
        struct tm *p = localtime(&sec);
        if (p) { tm_local = *p; } else { memset(&tm_local, 0, sizeof(tm_local)); }
    }
#else
#  error "No localtime variant configured"
#endif
    snprintf(buf, bufsz, "%04d-%02d-%02d %02d:%02d:%02d.%03u",
             tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
             tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, msec);
}

#if defined(_WIN32)
static CRITICAL_SECTION g_log_mutex;
static void pal_log_mutex_init(void)
{
    static LONG s_initialized = 0;
    if (InterlockedCompareExchange(&s_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_log_mutex);
    }
}
#  define PAL_LOG_LOCK()   do { pal_log_mutex_init(); EnterCriticalSection(&g_log_mutex); } while (0)
#  define PAL_LOG_UNLOCK() LeaveCriticalSection(&g_log_mutex)
#else
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
#  define PAL_LOG_LOCK()   pthread_mutex_lock(&g_log_mutex)
#  define PAL_LOG_UNLOCK() pthread_mutex_unlock(&g_log_mutex)
#endif

void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap)
{
    char line[PAL_LOG_LINE_BUF];
    char ts[32];
    char *p = line;
    size_t remaining = sizeof(line);
    int n;

    const char *color = level_color(level);
    const char *letter = level_letter(level);
    pal_log_now_str(ts, sizeof(ts));
    pal_log_tid_t tid = pal_log_gettid();
    const char *tag_safe = tag ? tag : "?";

    n = snprintf(p, remaining, "[%s] [TID:" PAL_LOG_TID_FMT "] %s[%s] [%s] ",
                 ts, (unsigned long)tid, color, letter, tag_safe);
    if (n < 0) { return; }
    if ((size_t)n >= remaining) { p[sizeof(line) - 2] = '\n'; p[sizeof(line) - 1] = '\0'; goto emit; }
    p += n; remaining -= (size_t)n;

    n = vsnprintf(p, remaining, fmt, ap);
    if (n < 0) { return; }
    if ((size_t)n >= remaining) { p = line + sizeof(line) - 1; }
    else { p += n; remaining -= (size_t)n; }

    n = snprintf(p, remaining, ANSI_RESET "\n");
    if (n < 0) { return; }
    (void)n;

emit:
    PAL_LOG_LOCK();
    fwrite(line, 1, strlen(line), stderr);
    fflush(stderr);
    PAL_LOG_UNLOCK();
}

bool pal_log_in_isr(void)
{
    return pal_os_in_sim_isr_context();
}

void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap)
{
    char line[256];
    int n;
    const char *tag_safe = tag ? tag : "?";
    n = snprintf(line, sizeof(line), "!ISR! %s [%s] ",
                 level_letter(level), tag_safe);
    if (n < 0) { n = 0; }
    if ((size_t)n < sizeof(line) - 2) {
        int m = vsnprintf(line + n, sizeof(line) - (size_t)n - 1, fmt, ap);
        if (m >= 0) { n += m; }
    }
    if ((size_t)n < sizeof(line) - 1) { line[n++] = '\n'; }
    line[n] = '\0';
    size_t len = (size_t)n;

#if defined(_WIN32)
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, line, (DWORD)len, &written, NULL);
    }
#else
    (void)write(STDERR_FILENO, line, len);
#endif
}
