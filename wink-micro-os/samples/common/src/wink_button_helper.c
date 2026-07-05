/**
 * @file wink_button_helper.c
 * @brief Soft_timer-based button auto-poll helper (samples/common).
 *
 * Static slot pool (default 4, override via -DWINK_BUTTON_HELPER_MAX=N).
 * Each active slot owns a periodic soft_timer whose callback calls
 * dal_button_poll() on the tracked button.  Event callbacks registered
 * via dal_button_on_event() therefore run in the soft_timer dispatch
 * context (see wink_button_helper.h @warning).
 */
#define LOG_TAG "btn_helper"

#include "wink_button_helper.h"
#include "wink_soft_timer.h"
#include "wink_status.h"
#include "pal_log.h"

/* ── per-button slot ───────────────────────────────────────────
 * A slot with btn == NULL is free.  timer is only meaningful when
 * btn != NULL; BSS zero-init is sufficient (btn=NULL means free).  */
typedef struct {
    dal_button_t *btn;
    int32_t       timer;   /* soft_timer handle; valid iff btn != NULL */
} btn_helper_slot_t;

static btn_helper_slot_t s_slots[WINK_BUTTON_HELPER_MAX];

/* Soft_timer callback: poll the button.  Returns WINK_OK so the
 * periodic timer keeps running; transient poll errors are non-fatal
 * (next tick retries), and we deliberately discard them here. */
static wink_status_t btn_poll_tick(void *arg)
{
    dal_button_t *btn = (dal_button_t *)arg;
    WINK_IGNORE_RESULT(dal_button_poll(btn));
    return WINK_OK;
}

/* Find slot index for @p btn, or -1 if not tracked. */
static int find_slot(const dal_button_t *btn)
{
    for (int i = 0; i < WINK_BUTTON_HELPER_MAX; i++) {
        if (s_slots[i].btn == btn) {
            return i;
        }
    }
    return -1;
}

wink_status_t wink_button_helper_start(dal_button_t *btn, uint32_t poll_ms)
{
    if (btn == NULL || poll_ms == 0u) {
        LOG_D("start: invalid arg (btn=%p poll_ms=%u)",
              (void *)btn, (unsigned)poll_ms);
        return WINK_ERR_INVALID_ARG;
    }
    if (find_slot(btn) >= 0) {
        LOG_D("start: btn=%p already auto-polled", (void *)btn);
        return WINK_ERR_INVALID_STATE;
    }

    /* Find a free slot (btn == NULL). */
    int free_idx = -1;
    for (int i = 0; i < WINK_BUTTON_HELPER_MAX; i++) {
        if (s_slots[i].btn == NULL) {
            free_idx = i;
            break;
        }
    }
    if (free_idx < 0) {
        LOG_D("start: out of slots (%d)", WINK_BUTTON_HELPER_MAX);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    int32_t h = wink_soft_timer_create(btn_poll_tick, btn,
                                       WINK_TIMER_PERIODIC, poll_ms);
    if (h < 0) {
        LOG_D("start: soft_timer_create failed: %d", (int)h);
        return (wink_status_t)h;
    }
    wink_status_t st = wink_soft_timer_start(h);
    if (wink_status_is_error(st)) {
        LOG_D("start: soft_timer_start failed: %d", (int)st);
        WINK_IGNORE_RESULT(wink_soft_timer_stop(h));
        return st;
    }

    s_slots[free_idx].btn   = btn;
    s_slots[free_idx].timer = h;
    return WINK_OK;
}

wink_status_t wink_button_helper_stop(dal_button_t *btn)
{
    if (btn == NULL) {
        return WINK_OK;
    }
    int idx = find_slot(btn);
    if (idx < 0) {
        return WINK_OK;  /* not tracked: no-op */
    }
    WINK_IGNORE_RESULT(wink_soft_timer_stop(s_slots[idx].timer));
    s_slots[idx].btn = NULL;  /* mark slot free */
    return WINK_OK;
}
