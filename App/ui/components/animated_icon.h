#ifndef UI_ANIMATED_ICON_H
#define UI_ANIMATED_ICON_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  UI_ICON_LIGHT = 0,
  UI_ICON_LOCK,
  UI_ICON_FAST,
  UI_ICON_WIPER,
  UI_ICON_HAND_WHEEL,
  UI_ICON_BATTERY,
  UI_ICON_TYPE_COUNT
} ui_icon_type_t;

typedef struct {
  ui_icon_type_t type;
  uint16_t x, y;         /* Absolute upper-left pixel; complete 20x16 bounds required. */
  uint32_t period_ms;    /* Full animation cycle; must be nonzero. */
  bool visible;
  uint32_t phase_ms;     /* Initial phase, modulo period_ms. */
} ui_icon_config_t;

/* Caller-owned fixed storage. Fields are implementation state: use the API to
 * change them. Each instance shares read-only artwork but has its own clock.
 * All calls use the same foreground context and monotonically increasing us. */
typedef struct {
  ui_icon_config_t config;
  uint64_t anchor_us, phase_us, last_seen_us;
  int16_t visual_state; /* Opaque sampled state, interpreted by its icon. */
  bool initialized;
} ui_icon_t;

bool ui_icon_init(ui_icon_t *icon, const ui_icon_config_t *config, uint64_t now_us);
/* Update samples the demo timeline; render draws the last sample without time
 * or side effects. Update hidden components too so their clocks continue. */
bool ui_icon_update(ui_icon_t *icon, uint64_t now_us);
bool ui_icon_render(const ui_icon_t *icon, u8g2_t *u);
/* Convenience form: update then render; rendering a frame can call separately. */
bool ui_icon_draw(ui_icon_t *icon, u8g2_t *u, uint64_t now_us);
bool ui_icon_set_position(ui_icon_t *icon, uint16_t x, uint16_t y);
/* Preserves animation phase at now_us; rejects zero period or time reversal. */
bool ui_icon_set_period(ui_icon_t *icon, uint32_t period_ms, uint64_t now_us);
/* Hidden icons continue to advance; visibility does not reset their clock. */
bool ui_icon_set_visible(ui_icon_t *icon, bool visible);

#ifdef __cplusplus
}
#endif
#endif
