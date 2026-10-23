#include "oled_refresh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static unsigned long checks;
static uint32_t random_state = UINT32_C(0x4268a719);

static uint32_t NextRandom(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

/* Independent O(n^2) partition oracle: try every possible last interval
 * beginning at each changed column. It shares no greedy merge decisions. */
static uint16_t OraclePage(const uint8_t *a, const uint8_t *b,
                           uint16_t *transactions)
{
    uint16_t positions[OLED_REFRESH_WIDTH];
    uint16_t cost[OLED_REFRESH_WIDTH + 1U] = {0};
    uint16_t count[OLED_REFRESH_WIDTH + 1U] = {0};
    uint16_t n = 0U;
    for (uint16_t x = 0U; x < OLED_REFRESH_WIDTH; ++x) {
        if (a[x] != b[x]) {
            positions[n++] = x;
        }
    }
    for (uint16_t end = 1U; end <= n; ++end) {
        cost[end] = UINT16_MAX;
        count[end] = UINT16_MAX;
        for (uint16_t start = 0U; start < end; ++start) {
            uint16_t length=(uint16_t)(positions[end-1U]-positions[start]+1U);
            uint16_t chunks=(uint16_t)((length+31U)/32U);
            uint16_t candidate = (uint16_t)(cost[start] + length + 7U*chunks);
            uint16_t txn = (uint16_t)(count[start] + 2U*chunks);
            if (candidate < cost[end]
                || (candidate == cost[end] && txn < count[end])) {
                cost[end] = candidate;
                count[end] = txn;
            }
        }
    }
    *transactions = count[n];
    return cost[n];
}

static void CheckPlan(const uint8_t *image, const uint8_t *baseline)
{
    struct {
        uint32_t before;
        OLED_RefreshPlan plan;
        uint32_t after;
    } guarded = {0};
    guarded.before=UINT32_C(0xabcdef12); guarded.after=UINT32_C(0x12345678);
    uint8_t applied[OLED_REFRESH_FRAME_BYTES];
    memcpy(applied, baseline, sizeof(applied));
    OLED_RefreshResult result = OLED_RefreshBuildPlan(image, baseline, true,
                                                     OLED_REFRESH_DIFF, &guarded.plan);
    OLED_RefreshPlan *plan = &guarded.plan;
    CHECK(guarded.before == UINT32_C(0xabcdef12));
    CHECK(guarded.after == UINT32_C(0x12345678));
    CHECK(plan->span_count <= OLED_REFRESH_MAX_SPANS);
    uint16_t bytes = 0U;
    uint16_t chunks = 0U;
    uint16_t changed = 0U;
    uint16_t oracle_wire = 0U;
    uint16_t oracle_txn = 0U;
    for (size_t i = 0U; i < OLED_REFRESH_FRAME_BYTES; ++i) {
        if (image[i] != baseline[i]) {
            ++changed;
        }
    }
    for (size_t page = 0U; page < OLED_REFRESH_PAGES; ++page) {
        uint16_t txn = 0U;
        oracle_wire = (uint16_t)(oracle_wire + OraclePage(image + page * 128U,
                                                          baseline + page * 128U, &txn));
        oracle_txn = (uint16_t)(oracle_txn + txn);
    }
    for (uint16_t i = 0U; i < plan->span_count; ++i) {
        const OLED_RefreshSpan *span = &plan->spans[i];
        CHECK(span->page < OLED_REFRESH_PAGES);
        CHECK(span->column < OLED_REFRESH_WIDTH);
        CHECK(span->length != 0U);
        CHECK((uint16_t)span->column + span->length <= OLED_REFRESH_WIDTH);
        if (i != 0U) {
            const OLED_RefreshSpan *prev = &plan->spans[i - 1U];
            CHECK(span->page > prev->page
                  || (span->page == prev->page
                      && span->column >= (uint16_t)prev->column + prev->length));
        }
        size_t offset = (size_t)span->page * OLED_REFRESH_WIDTH + span->column;
        memcpy(applied + offset, image + offset, span->length);
        bytes = (uint16_t)(bytes + span->length);
        chunks = (uint16_t)(chunks + (span->length+31U)/32U);
    }
    CHECK(memcmp(applied, image, sizeof(applied)) == 0);
    CHECK(changed == plan->changed_bytes);
    CHECK(bytes == plan->image_bytes);
    CHECK(plan->wire_bytes == bytes + 7U * chunks);
    CHECK(plan->transactions == 2U * chunks);
    /* Gap-merging is deliberately a bounded heuristic under 32-byte chunks;
       the independent optimum is a lower bound, not an optimality promise. */
    CHECK(plan->wire_bytes >= oracle_wire);
    CHECK(plan->wire_bytes <= 1248U);
    if(plan->wire_bytes == oracle_wire) CHECK(plan->transactions >= oracle_txn);
    CHECK(result == (changed != 0U ? OLED_REFRESH_OK : OLED_REFRESH_NO_CHANGE));
    ++checks;
}

static void TestPlanner(void)
{
    uint8_t baseline[OLED_REFRESH_FRAME_BYTES] = {0};
    uint8_t image[OLED_REFRESH_FRAME_BYTES] = {0};
    OLED_RefreshPlan plan;
    CHECK(OLED_RefreshBuildPlan(NULL, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshBuildPlan(image, NULL, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, (OLED_RefreshMode)99, &plan)
          == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshBuildPlan(image, NULL, false, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.full && plan.image_bytes == 1024U && plan.wire_bytes == 1248U
          && plan.transactions == 64U && plan.span_count == 8U);
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_FULL, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.full && plan.changed_bytes == 0U && plan.image_bytes == 1024U);
    CheckPlan(image, baseline);
    image[0] = 1U;
    image[127] = 1U;
    image[128] = 1U;
    image[1023] = 1U;
    CheckPlan(image, baseline);
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.span_count == 4U && plan.wire_bytes == 32U);

    memset(image, 0, sizeof(image));
    image[10] = 1U;
    image[18] = 1U; /* Gap seven: merge at equal byte cost, saving 2 txns. */
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.span_count == 1U && plan.spans[0].length == 9U);
    image[18] = 0U;
    image[19] = 1U; /* Gap eight: split is cheaper by one wire byte. */
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.span_count == 2U && plan.wire_bytes == 16U);

    memset(image, 0, sizeof(image));
    for (size_t page = 0U; page < OLED_REFRESH_PAGES; ++page) {
        for (size_t x = 0U; x < OLED_REFRESH_WIDTH; x += 9U) {
            image[page * OLED_REFRESH_WIDTH + x] = UINT8_MAX;
        }
    }
    CheckPlan(image, baseline); /* Maximum 120 disjoint economical spans. */
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.span_count == 120U);
    memset(image, UINT8_MAX, sizeof(image));
    CheckPlan(image, baseline);
    CHECK(OLED_RefreshBuildPlan(image, baseline, true, OLED_REFRESH_DIFF, &plan)
          == OLED_REFRESH_OK);
    CHECK(plan.full && plan.span_count == 8U && plan.transactions == 64U);

    memset(image, 0, sizeof(image));
    for (uint32_t mask = 0U; mask < UINT32_C(65536); ++mask) {
        for (uint16_t x = 0U; x < 16U; ++x) {
            image[56U + x] = (uint8_t)((mask >> x) & 1U);
        }
        CheckPlan(image, baseline);
    }
    for (uint32_t iteration = 0U; iteration < 5000U; ++iteration) {
        uint32_t threshold = NextRandom() & 255U;
        for (size_t i = 0U; i < sizeof(image); ++i) {
            baseline[i] = (uint8_t)NextRandom();
            image[i] = baseline[i];
            if ((NextRandom() & 255U) < threshold) {
                image[i] ^= (uint8_t)((NextRandom() & 255U) | 1U);
            }
        }
        CheckPlan(image, baseline);
    }
}

static OLED_FrameMeta Meta(uint32_t seq)
{
    OLED_FrameMeta meta = {0};
    meta.phase = "HOST";
    meta.seq = seq;
    meta.sample_us = UINT64_C(0x100000000) + (uint64_t)seq * 20000U;
    meta.publish_us = meta.sample_us + 123U;
    meta.draw_us = 123U;
    return meta;
}

static uint8_t *Publish(OLED_Refresh *core, uint8_t value, uint32_t seq)
{
    uint8_t *image = NULL;
    OLED_FrameMeta meta = Meta(seq);
    CHECK(OLED_RefreshBeginDraw(core, &image) == OLED_REFRESH_OK);
    CHECK(image != NULL);
    memset(image, value, OLED_REFRESH_FRAME_BYTES);
    CHECK(OLED_RefreshPublish(core, &meta) == OLED_REFRESH_OK);
    return image;
}

static void TestOwnership(void)
{
    OLED_Refresh core;
    OLED_RefreshInit(&core, OLED_REFRESH_DIFF);
    CHECK(!core.confirmed_valid && !OLED_RefreshIsActive(&core));
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_NOT_READY);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_NOT_READY);
    OLED_FrameMeta meta = Meta(1U);
    CHECK(OLED_RefreshPublish(&core, &meta) == OLED_REFRESH_NOT_READY);
    uint8_t *original = Publish(&core, 0xAAU, 1U);
    CHECK(OLED_RefreshHasPending(&core));
    CHECK(OLED_RefreshSetMode(&core, OLED_REFRESH_FULL) == OLED_REFRESH_BUSY);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshActiveImage(&core) == original);
    CHECK(core.plan.full && !core.confirmed_valid);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_BUSY);
    CHECK(OLED_RefreshInvalidate(&core) == OLED_REFRESH_BUSY);
    CHECK(OLED_RefreshSetMode(&core, OLED_REFRESH_FULL) == OLED_REFRESH_BUSY);
    CHECK(OLED_RefreshActiveMeta(&core)->sample_us == meta.sample_us);
    uint8_t *next = Publish(&core, 0x55U, 2U);
    CHECK(next != original);
    CHECK(OLED_RefreshActiveImage(&core)[0] == 0xAAU);
    CHECK(Publish(&core, 0x33U, 3U) == next);
    CHECK(core.replaced == 1U && core.pending_meta.seq == 3U);
    CHECK(core.active_meta.seq == 1U);
    CHECK(OLED_RefreshActiveImage(&core)[1023] == 0xAAU);
    CHECK(!core.confirmed_valid); /* A publish is never screen completion. */
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
    CHECK(core.confirmed_valid && core.confirmed[0] == 0xAAU);
    CHECK(core.confirmed[1023] == 0xAAU);
    CHECK(OLED_RefreshActiveImage(&core) == NULL);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshActiveImage(&core) == next);
    CHECK(core.active_meta.seq == 3U);
    CHECK(core.active_meta.publish_us == Meta(3U).publish_us);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
    Publish(&core, 0x33U, 4U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_NO_CHANGE);
    CHECK(!core.active && !core.pending && core.unchanged == 1U);
    CHECK(core.plan.span_count == 0U && core.active_meta.seq == 4U);
    CHECK(core.active_meta.publish_us == Meta(4U).publish_us);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_NOT_READY);
    CHECK(OLED_RefreshSetMode(&core, OLED_REFRESH_FULL) == OLED_REFRESH_OK);
    Publish(&core, 0x33U, 5U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.full && core.plan.changed_bytes == 0U);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
    CHECK(core.unchanged == 1U);

    uint8_t *drawing = NULL;
    CHECK(OLED_RefreshBeginDraw(&core, &drawing) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshBeginDraw(&core, &next) == OLED_REFRESH_BUSY);
    CHECK(next == NULL);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_BUSY);
    CHECK(OLED_RefreshSetMode(&core, OLED_REFRESH_DIFF) == OLED_REFRESH_BUSY);
    meta = Meta(6U);
    meta.publish_us = meta.sample_us - 1U;
    CHECK(OLED_RefreshPublish(&core, &meta) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(core.writing && !core.pending);
    meta = Meta(6U);
    memset(drawing, 0U, OLED_REFRESH_FRAME_BYTES);
    CHECK(OLED_RefreshPublish(&core, &meta) == OLED_REFRESH_OK);
}

static void TestRecovery(void)
{
    OLED_Refresh core;
    OLED_RefreshInit(&core, OLED_REFRESH_DIFF);
    Publish(&core, 0U, 1U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
    uint8_t *image = NULL;
    CHECK(OLED_RefreshBeginDraw(&core, &image) == OLED_REFRESH_OK);
    memset(image, 0U, OLED_REFRESH_FRAME_BYTES);
    image[500] = 1U;
    OLED_FrameMeta meta = Meta(2U);
    CHECK(OLED_RefreshPublish(&core, &meta) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.span_count == 1U && core.plan.image_bytes == 1U);
    CHECK(core.confirmed[500] == 0U);
    CHECK(OLED_RefreshComplete(&core, false) == OLED_REFRESH_OK);
    CHECK(!core.confirmed_valid && core.pending);
    CHECK(core.pending_meta.seq == 2U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.full && core.plan.image_bytes == 1024U);
    CHECK(OLED_RefreshActiveImage(&core)[500] == 1U);
    /* Newer complete pending picture takes precedence over the failed one. */
    Publish(&core, 0xCCU, 3U);
    CHECK(OLED_RefreshComplete(&core, false) == OLED_REFRESH_OK);
    CHECK(core.pending_meta.seq == 3U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.full && core.active_meta.seq == 3U);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
    CHECK(core.confirmed_valid && core.confirmed[500] == 0xCCU);
    CHECK(OLED_RefreshInvalidate(&core) == OLED_REFRESH_OK);
    Publish(&core, 0xCCU, 4U);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.full); /* Same bytes must still resync an unknown panel. */
    CHECK(OLED_RefreshBeginDraw(&core, &image) == OLED_REFRESH_OK);
    memset(image, 0xDDU, OLED_REFRESH_FRAME_BYTES);
    CHECK(OLED_RefreshComplete(&core, false) == OLED_REFRESH_OK);
    CHECK(core.writing && !core.pending);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_BUSY);
    meta = Meta(5U);
    CHECK(OLED_RefreshPublish(&core, &meta) == OLED_REFRESH_OK);
    CHECK(OLED_RefreshStart(&core) == OLED_REFRESH_OK);
    CHECK(core.plan.full && core.active_meta.seq == 5U);
    CHECK(OLED_RefreshActiveImage(&core)[500] == 0xDDU);
    CHECK(OLED_RefreshComplete(&core, true) == OLED_REFRESH_OK);
}

static void TestInvalidArguments(void)
{
    OLED_Refresh core;
    OLED_RefreshInit(&core, OLED_REFRESH_DIFF);
    uint8_t *image = NULL;
    OLED_FrameMeta meta = Meta(1U);
    OLED_RefreshInit(NULL, OLED_REFRESH_FULL);
    CHECK(OLED_RefreshBeginDraw(NULL, &image) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshBeginDraw(&core, NULL) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshPublish(NULL, &meta) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshPublish(&core, NULL) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshStart(NULL) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshComplete(NULL, true) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshInvalidate(NULL) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshSetMode(NULL, OLED_REFRESH_DIFF) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshSetMode(&core, (OLED_RefreshMode)9) == OLED_REFRESH_BAD_ARGUMENT);
    CHECK(OLED_RefreshActiveImage(NULL) == NULL);
    CHECK(OLED_RefreshActiveMeta(NULL) == NULL);
    CHECK(OLED_RefreshGetPlan(NULL) == NULL);
    CHECK(!OLED_RefreshHasPending(NULL));
    CHECK(!OLED_RefreshIsActive(NULL));
}

int main(void)
{
    TestPlanner();
    TestOwnership();
    TestRecovery();
    TestInvalidArguments();
    printf("PASS refresh: %lu planner cases (65536 exhaustive masks, 5000 random frames), "
           "independent cost/transaction oracle, bounds/reconstruction, ownership, "
           "latest replacement, no-change, failure/full resync, argument guards.\n", checks);
    printf("Host sizeof OLED_Refresh=%lu, frame buffers=2048, confirmed shadow=1024 bytes.\n",
           (unsigned long)sizeof(OLED_Refresh));
    return EXIT_SUCCESS;
}
