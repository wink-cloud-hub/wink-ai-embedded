/**
 * @file selftest_core.c
 * @brief Selftest 框架：X-macro 注册表 + 简单 glob 匹配 + 批量执行。
 */
#define LOG_TAG "wink_selftest"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"

#include <stddef.h>
#include <string.h>

/* ── 注册表构造（X-macro） ──────────────────────────────── */
typedef struct {
    const char       *name;
    wink_selftest_fn  fn;
} wink_selftest_entry_t;

#define WINK_SELFTEST_ENTRY(entry, name_str, fn) { name_str, fn },
static const wink_selftest_entry_t s_registry[] = {
#include "wink_selftest_registry.def"
};
#undef WINK_SELFTEST_ENTRY

#define WINK_SELFTEST_REGISTRY_SIZE \
    (sizeof(s_registry) / sizeof(s_registry[0]))

/* ── 简单 glob 匹配：支持 * 作为前缀/后缀/全匹配 ──────── */
/* "*"      匹配所有
 * "xxx*"   前缀匹配
 * "*xxx"   后缀匹配
 * "xxx"    完全匹配
 * 不支持 ?、字符类、多 * */
static bool glob_match(const char *glob, const char *s)
{
    if (glob == NULL || s == NULL) return false;
    size_t glen = strlen(glob);
    size_t slen = strlen(s);

    if (glen == 1 && glob[0] == '*') return true;  /* "*" = all */

    if (glen > 0 && glob[0] == '*' && glob[glen-1] == '*') {
        /* *xxx* infix (not used currently but support) */
        if (glen < 3) return true;  /* "**" */
        char buf[64];
        size_t mid_len = glen - 2;
        if (mid_len >= sizeof(buf)) return false;
        memcpy(buf, glob + 1, mid_len);
        buf[mid_len] = '\0';
        return strstr(s, buf) != NULL;
    }
    if (glen > 0 && glob[glen-1] == '*') {
        /* prefix: "xxx*" */
        size_t pfx = glen - 1;
        return slen >= pfx && strncmp(glob, s, pfx) == 0;
    }
    if (glen > 0 && glob[0] == '*') {
        /* suffix: "*xxx" */
        size_t sfx = glen - 1;
        return slen >= sfx && strcmp(s + slen - sfx, glob + 1) == 0;
    }
    /* exact */
    return strcmp(glob, s) == 0;
}

/* ── 公共 API ───────────────────────────────────────────── */
size_t wink_selftest_count(void)
{
    return WINK_SELFTEST_REGISTRY_SIZE;
}

wink_status_t wink_selftest_run(const char *name_glob,
                                wink_selftest_result_t *results,
                                size_t cap,
                                size_t *out_count)
{
    if (name_glob == NULL) return WINK_ERR_INVALID_ARG;

    size_t matched = 0;
    wink_status_t first_fail = WINK_OK;

    for (size_t i = 0; i < WINK_SELFTEST_REGISTRY_SIZE; i++) {
        const wink_selftest_entry_t *e = &s_registry[i];
        if (!glob_match(name_glob, e->name)) continue;

        /* 准备结果槽 */
        wink_selftest_result_t tmp;
        tmp.name   = e->name;
        tmp.status = WINK_OK;
        tmp.metric = 0;
        tmp.note   = NULL;

        /* 执行条目 */
        wink_status_t st = e->fn(&tmp);
        tmp.status = st;

        /* 记录日志 */
        const char *verdict;
        if (st == WINK_OK) {
            verdict = "PASS";
        } else if (st == WINK_ERR_UNSUPPORTED) {
            verdict = "SKIP";
        } else {
            verdict = "FAIL";
            if (first_fail == WINK_OK) first_fail = st;
        }
        if (tmp.note != NULL) {
            LOG_I("[selftest] %s: %s metric=%lu (%s)",
                  e->name, verdict, (unsigned long)tmp.metric, tmp.note);
        } else {
            LOG_I("[selftest] %s: %s metric=%lu",
                  e->name, verdict, (unsigned long)tmp.metric);
        }

        /* 回填到调用方数组 */
        if (results != NULL && matched < cap) {
            results[matched] = tmp;
        }
        matched++;
    }

    if (out_count != NULL) *out_count = matched;
    return first_fail;
}
