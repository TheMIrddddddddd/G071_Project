#ifndef SHOWCASE_MODEL_H
#define SHOWCASE_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SHOWCASE_ALGORITHMS 6U
#define SHOWCASE_STICK_STAGE_US UINT64_C(12000000)
#define SHOWCASE_STICKS_US (SHOWCASE_ALGORITHMS * SHOWCASE_STICK_STAGE_US)
#define SHOWCASE_MENU_ACTIONS 25U
#define SHOWCASE_MENU_ACTION_US UINT64_C(1100000)
#define SHOWCASE_MENU_US UINT64_C(29000000)
#define SHOWCASE_CYCLE_US (SHOWCASE_STICKS_US + SHOWCASE_MENU_US)
#define SHOWCASE_UPDATE_HZ 75U
#define SHOWCASE_ASTRA_SPEED 73U
typedef enum { SHOWCASE_STICKS, SHOWCASE_ASTRA } ShowcaseSection;
typedef struct {
    ShowcaseSection section;
    uint32_t cycle, section_ms, updates, skipped_ticks, renders;
    uint8_t algorithm, gesture, menu_page, menu_index, menu_action, popup;
    float current[6], target[6];
    float camera_x, camera_y, selector_x, selector_y, selector_w, selector_h;
} ShowcaseSnapshot;
/* One foreground caller. Menu first (29s), then six stick algorithms (72s).
 * Init constructs the fixed tree once. Update advances model state only;
 * Draw rasterizes the latest state once and caches it until the next update.
 * Update/Draw never allocate, block, call a peripheral or touch DMA storage. */
bool Showcase_Init(uint64_t now_us);
/* G071 port: borrow a caller-owned bootstrap buffer instead of an extra 1KB
   static. Drawing later binds the model's own frame, preserving pixels. */
bool Showcase_InitWithBuffer(uint64_t now_us, uint8_t *bootstrap);
bool Showcase_Update(uint64_t now_us);
bool Showcase_Draw(uint8_t *frame);
bool Showcase_GetSnapshot(ShowcaseSnapshot *out);
const char *Showcase_AlgorithmName(unsigned algorithm);
#ifdef __cplusplus
}
#endif
#endif
