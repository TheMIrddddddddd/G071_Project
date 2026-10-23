#include "oled_refresh.h"

#include <string.h>

static bool ModeValid(OLED_RefreshMode mode)
{
    return mode == OLED_REFRESH_FULL || mode == OLED_REFRESH_DIFF;
}

static void AppendSpan(OLED_RefreshPlan *plan, uint8_t page,
                       uint8_t column, uint16_t length)
{
    OLED_RefreshSpan *span = &plan->spans[plan->span_count];
    span->page = page;
    span->column = column;
    span->length = length;
    plan->span_count++;
    plan->image_bytes = (uint16_t)(plan->image_bytes + length);
    uint16_t chunks = (uint16_t)((length + OLED_REFRESH_CHUNK_BYTES - 1U) / OLED_REFRESH_CHUNK_BYTES);
    plan->wire_bytes = (uint16_t)(plan->wire_bytes + length + chunks * OLED_REFRESH_SPAN_OVERHEAD);
    plan->transactions = (uint16_t)(plan->transactions + 2U * chunks);
}

static void FullPlan(OLED_RefreshPlan *plan)
{
    plan->span_count = 0U;
    plan->image_bytes = 0U;
    plan->wire_bytes = 0U;
    plan->transactions = 0U;
    for (uint8_t page = 0U; page < OLED_REFRESH_PAGES; ++page) {
        AppendSpan(plan, page, 0U, OLED_REFRESH_WIDTH);
    }
    plan->full = true;
}

OLED_RefreshResult OLED_RefreshBuildPlan(const uint8_t *image,
                                        const uint8_t *confirmed,
                                        bool confirmed_valid,
                                        OLED_RefreshMode mode,
                                        OLED_RefreshPlan *plan)
{
    if (image == NULL || plan == NULL || !ModeValid(mode)
        || (confirmed_valid && confirmed == NULL)) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    if (confirmed_valid) {
        for (size_t i = 0U; i < OLED_REFRESH_FRAME_BYTES; ++i) {
            if (image[i] != confirmed[i]) {
                plan->changed_bytes++;
            }
        }
    } else {
        /* Unknown screen means all 1024 bytes require synchronization; this
         * count does not claim 1024 measured changes against unknown pixels. */
        plan->changed_bytes = OLED_REFRESH_FRAME_BYTES;
    }
    if (!confirmed_valid || mode == OLED_REFRESH_FULL) {
        FullPlan(plan);
        return OLED_REFRESH_OK;
    }
    if (plan->changed_bytes == 0U) {
        return OLED_REFRESH_NO_CHANGE;
    }

    for (uint8_t page = 0U; page < OLED_REFRESH_PAGES; ++page) {
        size_t base = (size_t)page * OLED_REFRESH_WIDTH;
        uint16_t column = 0U;
        while (column < OLED_REFRESH_WIDTH) {
            while (column < OLED_REFRESH_WIDTH
                   && image[base + column] == confirmed[base + column]) {
                ++column;
            }
            if (column == OLED_REFRESH_WIDTH) {
                break;
            }
            uint16_t first = column;
            uint16_t last = column;
            ++column;
            for (; column < OLED_REFRESH_WIDTH; ++column) {
                if (image[base + column] != confirmed[base + column]) {
                    /* A new span costs seven bytes. Merge gaps up to seven
                     * bytes; at equality fewer transactions wins the tie.
                     * This bounded merging heuristic is not a global optimum
                     * under chunking; actual chunk costs are checked below. */
                    if (column - last - 1U > OLED_REFRESH_SPAN_OVERHEAD) {
                        break;
                    }
                    last = column;
                }
            }
            /* More than 128 spans cannot occur: separated runs need >=8
             * unchanged bytes, giving at most 15 spans/page, 120/frame.
             * Keep a defensive full fallback for future protocol changes. */
            if (plan->span_count == OLED_REFRESH_MAX_SPANS) {
                FullPlan(plan);
                return OLED_REFRESH_OK;
            }
            AppendSpan(plan, page, (uint8_t)first, (uint16_t)(last - first + 1U));
        }
    }
    /* Sparse changes spanning a whole page may already become one 128-byte
     * span. A fully spread frame at full cost takes exactly the full path. */
    if (plan->wire_bytes >= OLED_REFRESH_FRAME_BYTES
        + OLED_REFRESH_PAGES * (OLED_REFRESH_WIDTH / OLED_REFRESH_CHUNK_BYTES) * OLED_REFRESH_SPAN_OVERHEAD) {
        FullPlan(plan);
    }
    return OLED_REFRESH_OK;
}

void OLED_RefreshInit(OLED_Refresh *refresh, OLED_RefreshMode mode)
{
    if (refresh != NULL) {
        memset(refresh, 0, sizeof(*refresh));
        refresh->mode = ModeValid(mode) ? mode : OLED_REFRESH_DIFF;
        refresh->active_slot = 1U;
    }
}

OLED_RefreshResult OLED_RefreshBeginDraw(OLED_Refresh *refresh, uint8_t **image)
{
    if (refresh == NULL || image == NULL) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    *image = NULL;
    if (refresh->writing) {
        return OLED_REFRESH_BUSY;
    }
    if (refresh->pending) {
        refresh->pending = false;
        refresh->replaced++;
    }
    refresh->writing = true;
    *image = refresh->frames[refresh->draw_slot];
    return OLED_REFRESH_OK;
}

OLED_RefreshResult OLED_RefreshPublish(OLED_Refresh *refresh,
                                     const OLED_FrameMeta *meta)
{
    if (refresh == NULL || meta == NULL || meta->publish_us < meta->sample_us) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    if (!refresh->writing) {
        return OLED_REFRESH_NOT_READY;
    }
    refresh->pending_meta = *meta;
    refresh->writing = false;
    refresh->pending = true;
    return OLED_REFRESH_OK;
}

OLED_RefreshResult OLED_RefreshCancelDraw(OLED_Refresh *refresh)
{
    if (!refresh) return OLED_REFRESH_BAD_ARGUMENT;
    if (!refresh->writing) return OLED_REFRESH_NOT_READY;
    refresh->writing = false;
    return OLED_REFRESH_OK;
}

OLED_RefreshResult OLED_RefreshStart(OLED_Refresh *refresh)
{
    if (refresh == NULL) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    if (refresh->active || refresh->writing) {
        return OLED_REFRESH_BUSY;
    }
    if (!refresh->pending) {
        return OLED_REFRESH_NOT_READY;
    }
    uint8_t old_active = refresh->active_slot;
    refresh->active_slot = refresh->draw_slot;
    refresh->draw_slot = old_active;
    refresh->active_meta = refresh->pending_meta;
    refresh->pending = false;
    OLED_RefreshResult result = OLED_RefreshBuildPlan(
        refresh->frames[refresh->active_slot], refresh->confirmed,
        refresh->confirmed_valid, refresh->mode, &refresh->plan);
    if (result == OLED_REFRESH_NO_CHANGE) {
        refresh->unchanged++;
    } else if (result == OLED_REFRESH_OK) {
        refresh->active = true;
    }
    return result;
}

OLED_RefreshResult OLED_RefreshComplete(OLED_Refresh *refresh, bool success)
{
    if (refresh == NULL) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    if (!refresh->active) {
        return OLED_REFRESH_NOT_READY;
    }
    if (success) {
        memcpy(refresh->confirmed, refresh->frames[refresh->active_slot],
               OLED_REFRESH_FRAME_BYTES);
        refresh->confirmed_valid = true;
    } else {
        refresh->confirmed_valid = false;
        if (!refresh->pending && !refresh->writing) {
            uint8_t old_draw = refresh->draw_slot;
            refresh->draw_slot = refresh->active_slot;
            refresh->active_slot = old_draw;
            refresh->pending_meta = refresh->active_meta;
            refresh->pending = true;
        }
    }
    refresh->active = false;
    return OLED_REFRESH_OK;
}

OLED_RefreshResult OLED_RefreshInvalidate(OLED_Refresh *refresh)
{
    if (refresh == NULL) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    if (refresh->active) {
        return OLED_REFRESH_BUSY;
    }
    refresh->confirmed_valid = false;
    return OLED_REFRESH_OK;
}

OLED_RefreshResult OLED_RefreshSetMode(OLED_Refresh *refresh, OLED_RefreshMode mode)
{
    if (refresh == NULL || !ModeValid(mode)) {
        return OLED_REFRESH_BAD_ARGUMENT;
    }
    if (refresh->active || refresh->writing || refresh->pending) {
        /* A published picture must retain its original strategy attribution. */
        return OLED_REFRESH_BUSY;
    }
    refresh->mode = mode;
    return OLED_REFRESH_OK;
}

const uint8_t *OLED_RefreshActiveImage(const OLED_Refresh *refresh)
{
    return refresh != NULL && refresh->active
        ? refresh->frames[refresh->active_slot] : NULL;
}

const OLED_FrameMeta *OLED_RefreshActiveMeta(const OLED_Refresh *refresh)
{
    return refresh != NULL ? &refresh->active_meta : NULL;
}

const OLED_RefreshPlan *OLED_RefreshGetPlan(const OLED_Refresh *refresh)
{
    return refresh != NULL ? &refresh->plan : NULL;
}

bool OLED_RefreshHasPending(const OLED_Refresh *refresh)
{
    return refresh != NULL && refresh->pending;
}

bool OLED_RefreshIsActive(const OLED_Refresh *refresh)
{
    return refresh != NULL && refresh->active;
}
