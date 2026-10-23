#ifndef OLED_REFRESH_H
#define OLED_REFRESH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oled_frame_info.h"

#define OLED_REFRESH_WIDTH 128U
#define OLED_REFRESH_PAGES 8U
#define OLED_REFRESH_FRAME_BYTES (OLED_REFRESH_WIDTH * OLED_REFRESH_PAGES)
#define OLED_REFRESH_MAX_SPANS 128U
#define OLED_REFRESH_SPAN_OVERHEAD 7U
#define OLED_REFRESH_CHUNK_BYTES 32U

typedef enum {
    OLED_REFRESH_FULL = 0,
    OLED_REFRESH_DIFF = 1
} OLED_RefreshMode;

typedef enum {
    OLED_REFRESH_OK = 0,
    OLED_REFRESH_BUSY,
    OLED_REFRESH_NO_CHANGE,
    OLED_REFRESH_BAD_ARGUMENT,
    OLED_REFRESH_NOT_READY
} OLED_RefreshResult;

typedef struct {
    const char *phase; /* Application-owned string with static lifetime. */
    uint32_t seq;
    uint64_t sample_us;
    uint64_t publish_us;
    uint32_t draw_us;
    OLED_AnimationInfo animation;
} OLED_FrameMeta;

typedef struct {
    uint8_t page;
    uint8_t column;
    uint16_t length;
} OLED_RefreshSpan;

typedef struct {
    OLED_RefreshSpan spans[OLED_REFRESH_MAX_SPANS];
    uint16_t span_count;
    uint16_t changed_bytes;
    uint16_t image_bytes;
    uint16_t wire_bytes;
    uint16_t transactions;
    bool full;
} OLED_RefreshPlan;

/* Persistent storage: two ownership-swapped frames plus a 1024-byte confirmed
 * screen shadow. The shadow is necessary because both frames may hold newer
 * pictures while the panel still contains the last completed update.
 * All functions are restricted to ONE foreground context; never call in ISR.
 * Fields are visible for static allocation, but mutate only via this API. */
typedef struct {
    uint8_t frames[2][OLED_REFRESH_FRAME_BYTES];
    uint8_t confirmed[OLED_REFRESH_FRAME_BYTES];
    OLED_RefreshPlan plan;
    OLED_FrameMeta pending_meta;
    OLED_FrameMeta active_meta;
    OLED_RefreshMode mode;
    uint32_t replaced;
    uint32_t unchanged;
    uint8_t draw_slot;
    uint8_t active_slot;
    bool pending;
    bool writing;
    bool active;
    bool confirmed_valid;
} OLED_Refresh;

/* Pure planner. Format: byte[page * 128 + x], bit 0 is y=page*8.
 * Spans are sent in <=32-byte chunks, each with a position command:
 * length+7*ceil(length/32) wire bytes, 2*ceil(length/32) transactions.
 * Wire counts exclude START/STOP bit timing, include address bytes/ACK clocks
 * indirectly when the caller multiplies wire_bytes by 9.
 * Unknown shadow and FULL mode always produce eight full-page spans. */
OLED_RefreshResult OLED_RefreshBuildPlan(const uint8_t *image,
                                        const uint8_t *confirmed,
                                        bool confirmed_valid,
                                        OLED_RefreshMode mode,
                                        OLED_RefreshPlan *plan);

void OLED_RefreshInit(OLED_Refresh *refresh, OLED_RefreshMode mode);

/* BeginDraw returns the only writable full-frame buffer. It discards/counts
 * an older pending picture before exposing storage for replacement. Redraw
 * the ENTIRE image; swapped storage may contain an older, unrelated frame.
 * The pointer is writable only until Publish, then reacquire via BeginDraw.
 * BeginDraw remains available while a different frame is being transmitted. */
OLED_RefreshResult OLED_RefreshBeginDraw(OLED_Refresh *refresh, uint8_t **image);
OLED_RefreshResult OLED_RefreshCancelDraw(OLED_Refresh *refresh);
OLED_RefreshResult OLED_RefreshPublish(OLED_Refresh *refresh,
                                     const OLED_FrameMeta *meta);

/* Start atomically (in foreground terms) selects the latest published frame,
 * swaps ownership, and plans it. OK means active/immutable; NO_CHANGE consumes
 * the pending frame without starting I2C, increments unchanged, not success.
 * ActiveMeta and GetPlan retain the most recent selection for logging.
 * A started frame is NOT confirmed until Complete(true). */
OLED_RefreshResult OLED_RefreshStart(OLED_Refresh *refresh);

/* Caller must prove last I2C data completion for success; on failure it must
 * stop DMA/I2C and prove no hardware reader remains BEFORE this function.
 * Failure invalidates shadow and retains the newest complete pending frame.
 * If none is pending/being drawn, the failed active frame is requeued.
 * No buffer is copied on submit/start; success copies 1024 bytes to shadow. */
OLED_RefreshResult OLED_RefreshComplete(OLED_Refresh *refresh, bool success);

/* Invalidating an in-flight update is forbidden; stop hardware then call
 * Complete(false). Invalidate while idle forces the next selection to FULL.
 * Mode switches require no active, pending or partially drawn frame. */
OLED_RefreshResult OLED_RefreshInvalidate(OLED_Refresh *refresh);
OLED_RefreshResult OLED_RefreshSetMode(OLED_Refresh *refresh, OLED_RefreshMode mode);

const uint8_t *OLED_RefreshActiveImage(const OLED_Refresh *refresh);
const OLED_FrameMeta *OLED_RefreshActiveMeta(const OLED_Refresh *refresh);
const OLED_RefreshPlan *OLED_RefreshGetPlan(const OLED_Refresh *refresh);
bool OLED_RefreshHasPending(const OLED_Refresh *refresh);
bool OLED_RefreshIsActive(const OLED_Refresh *refresh);

#ifdef __cplusplus
}
#endif
#endif
