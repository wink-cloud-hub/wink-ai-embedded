/**
 * @file pal_log_host.c
 * @brief Host target 分级日志后端：带时间戳/线程 ID/ANSI 颜色的 stderr 输出。
 *
 * 输出格式（单条原子写入，防多线程窜行）：
 *   [2026-07-05 07:22:15.123] [TID:0x4B3A] [E] [dal_servo] init failed (pin=12)
 *
 * 颜色映射（与 ESP-IDF 默认日志配色一致）：
 *   ERROR — 红底白字（高亮）
 *   WARN  — 黄色
 *   INFO  — 绿色
 *   DEBUG — 暗色
 *
 * 线程安全：
 *   - 单条日志先 vsnprintf 到栈上缓冲区，再通过一次 fwrite 写入 stderr，
 *     配合互斥锁保护临界区，彻底避免多线程并发 fprintf 时的字符交错。
 *
 * ISR 安全：
 *   - pal_log_isr_write() 提供无锁 ROM 路径：在仿真 ISR 上下文
 *     （pal_os_in_sim_isr_context()==true）下直接 write(2,...) 而不进入 stdio。
 *
 * ⚠️ ANSI 转义在 Windows 10+ 控制台（ConHost v2 / Windows Terminal）默认可用；
 *    旧版本 Windows 可能显示转义字符乱码——host target 主要面向开发机和 CI，
 *    不做 isatty() / ENABLE_VIRTUAL_TERMINAL_PROCESSING 检测（保持代码简单）。
 */
#include "pal_log.h"
#include "pal_osal.h"   /* pal_os_in_sim_isr_context() */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ---- 平台判定 --------------------------------------------------------
 * Win32 API detection (tid, mutex, write path, wall clock):
 *   _WIN32 → Windows API (CRITICAL_SECTION, GetCurrentThreadId, WriteFile,
 *            GetSystemTimeAsFileTime).
 *   else   → POSIX (pthread, gettimeofday, write).
 *
 * localtime variant detection:
 *   _MSC_VER           → MSVC localtime_s(&tm, &t) (security-enhanced).
 *   _POSIX_C_SOURCE>=1 || __GLIBC__ || __APPLE__ || __MINGW32__ && __MSVCRT_VERSION__>=0x1400
 *                      → POSIX localtime_r(&t, &tm).
 *   else (notably plain MinGW UCRT) → use localtime() — safe because caller
 *                      (pal_log_vprintf) already holds the global log mutex.
 */
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
   /* Fallback: plain localtime() — caller holds g_log_mutex so thread-safe. */
#  define PAL_LOG_HAVE_LOCALTIME_PLAIN 1
#endif

/* ---- ANSI 颜色 ------------------------------------------------------- */
#define ANSI_RESET   "\033[0m"
#define ANSI_RED_BG  "\033[41;37m"   /* ERROR: 红底白字 */
#define ANSI_YELLOW  "\033[33m"      /* WARN: 黄色 */
#define ANSI_GREEN   "\033[32m"      /* INFO: 绿色 */
#define ANSI_DIM     "\033[2m"       /* DEBUG: 暗色 */

#define PAL_LOG_LINE_BUF 768   /* 栈上单行缓冲区，含时间戳/TID/前缀/消息/换行 */

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

/* ---- 时钟：毫秒精度绝对时间戳 ---------------------------------------
 * Returns ms-since-epoch via a platform-specific wall clock, and fills buf
 * with "YYYY-MM-DD HH:MM:SS.mmm" local time. */
static void pal_log_now_str(char *buf, size_t bufsz)
{
    uint64_t ms;
    struct tm tm_local;

#if defined(_WIN32)
    /* Windows: GetSystemTimeAsFileTime (all Win32; no Win8+ requirement).
     * 100-ns intervals since 1601-01-01 UTC → Unix epoch ms. */
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
    /* Fallback: plain localtime() (returns thread-local or static buffer —
     * safe because our caller pal_log_vprintf() holds g_log_mutex). */
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

/* ---- 全局互斥锁：保护 stderr 临界区防窜行 --------------------------- */
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

/* ---- 后端主入口（同步路径）：单条原子写入 stderr -------------------- */
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

    /* 前缀：[ts] [TID:xxxx] [L] [tag]  —— 加 ANSI 颜色开关于 letter 前 */
    n = snprintf(p, remaining, "[%s] [TID:" PAL_LOG_TID_FMT "] %s[%s] [%s] ",
                 ts, (unsigned long)tid, color, letter, tag_safe);
    if (n < 0) { return; }
    if ((size_t)n >= remaining) { p[sizeof(line) - 2] = '\n'; p[sizeof(line) - 1] = '\0'; goto emit; }
    p += n; remaining -= (size_t)n;

    /* 消息体 */
    n = vsnprintf(p, remaining, fmt, ap);
    if (n < 0) { return; }
    if ((size_t)n >= remaining) { p = line + sizeof(line) - 1; /* 截断点已落在 p 起点 */ }
    else { p += n; remaining -= (size_t)n; }

    /* ANSI 重置 + 换行 */
    n = snprintf(p, remaining, ANSI_RESET "\n");
    if (n < 0) { return; }
    (void)n;

emit:
    /* 临界区：fwrite + fflush 作为单次原子操作序列 */
    PAL_LOG_LOCK();
    fwrite(line, 1, strlen(line), stderr);
    fflush(stderr);
    PAL_LOG_UNLOCK();
}

/* ---- ISR 探测（host：仿真 ISR 标志）--------------------------------- */
bool pal_log_in_isr(void)
{
    return pal_os_in_sim_isr_context();
}

/* ---- ISR 无锁路径：ERROR/WARN 专用，不走 stdio/不持锁 --------------
 *
 * Host 上 ISR 是仿真上下文（单纤维/单线程中调用），真正的并行不会发生；
 * 但为保持与 ESP32 路径语义一致，此路径仍然：
 *   1) 不获取 g_log_mutex；
 *   2) 不调用可能分配内存的 stdio（直接用 write(2) 到 fd=2）；
 *   3) 在栈上紧凑格式化。
 * 真实 POSIX 场景下 write(2) 对同一 fd 是原子的（≤PIPE_BUF 字节），
 * 多线程 host 测试里也不会出现字符交错。 */
void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap)
{
    char line[256];   /* ISR 路径更小的缓冲，避免占栈过多（真实 ISR 栈很有限） */
    int n;
    const char *tag_safe = tag ? tag : "?";
    n = snprintf(line, sizeof(line), "!ISR! %s [%s] ",
                 level_letter(level), tag_safe);
    if (n < 0) { n = 0; }
    if ((size_t)n < sizeof(line) - 2) {
        int m = vsnprintf(line + n, sizeof(line) - (size_t)n - 1, fmt, ap);
        if (m >= 0) { n += m; }
    }
    /* guarantee trailing \n\0 */
    if ((size_t)n < sizeof(line) - 1) { line[n++] = '\n'; }
    line[n] = '\0';
    size_t len = (size_t)n;

#if defined(_WIN32)
    /* Windows ISR 兜底：写 stderr 而不经过 stdio 锁定 */
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, line, (DWORD)len, &written, NULL);
    }
#else
    /* POSIX: write(2) 直接打到 fd 2，不经过 stdio buffer，无锁 */
    (void)write(STDERR_FILENO, line, len);
#endif
}
