#include "animated_icon.h"
#include "app_light_icon.h"
#include "app_lock_icon.h"
#include "app_fast_icon.h"
#include "app_wiper_icon.h"
#include "app_hand_wheel_icon.h"
#include "app_battery_icon.h"

namespace {
constexpr uint16_t screen_width = 128, screen_height = 64;
constexpr uint16_t icon_width = 20, icon_height = 16;
constexpr uint32_t original_period_ms[] = {2760U,1960U,2520U,6400U,3200U,6640U};
static_assert(sizeof(original_period_ms) / sizeof(original_period_ms[0]) == UI_ICON_TYPE_COUNT,
              "Every icon needs its original cycle duration");

bool position_valid(uint16_t x, uint16_t y)
{ return x <= screen_width-icon_width && y <= screen_height-icon_height; }

bool type_valid(ui_icon_type_t type)
{ return static_cast<unsigned>(type) < UI_ICON_TYPE_COUNT; }

uint64_t cycle_us(const ui_icon_t *icon)
{ return static_cast<uint64_t>(icon->config.period_ms) * 1000U; }

uint64_t phase_at(const ui_icon_t *icon, uint64_t now_us)
{
  const uint64_t cycle = cycle_us(icon);
  return ((now_us-icon->anchor_us)%cycle + icon->phase_us)%cycle;
}
}

bool ui_icon_init(ui_icon_t *icon, const ui_icon_config_t *config, uint64_t now_us)
{
  if (!icon || !config || !type_valid(config->type) || config->period_ms == 0U ||
      !position_valid(config->x, config->y)) return false;
  ui_icon_t next{};
  next.config = *config;
  next.anchor_us = next.last_seen_us = now_us;
  next.phase_us = (static_cast<uint64_t>(config->phase_ms) % config->period_ms)*1000U;
  next.initialized = true;
  *icon = next;
  return ui_icon_update(icon,now_us);
}

bool ui_icon_update(ui_icon_t *icon, uint64_t now_us)
{
  if (!icon || !icon->initialized || now_us < icon->last_seen_us) return false;
  icon->last_seen_us = now_us;
  // Multiplying by the original period in milliseconds stays within uint64_t
  // even for the maximum uint32_t configured period and retains microseconds.
  const uint64_t elapsed = phase_at(icon,now_us) * original_period_ms[icon->config.type]
                           / icon->config.period_ms;
  switch (icon->config.type) {
    case UI_ICON_LIGHT: icon->visual_state=app_light_icon_sample(elapsed); break;
    case UI_ICON_LOCK: icon->visual_state=app_lock_icon_sample(elapsed); break;
    case UI_ICON_FAST: icon->visual_state=app_fast_icon_sample(elapsed); break;
    case UI_ICON_WIPER: icon->visual_state=app_wiper_icon_sample(elapsed); break;
    case UI_ICON_HAND_WHEEL: icon->visual_state=app_hand_wheel_icon_sample(elapsed); break;
    case UI_ICON_BATTERY: icon->visual_state=app_battery_icon_sample(elapsed); break;
    default: return false;
  }
  return true;
}

bool ui_icon_render(const ui_icon_t *icon, u8g2_t *u)
{
  if (!icon || !icon->initialized || !u) return false;
  if (!icon->config.visible) return true;
  const auto x=static_cast<uint8_t>(icon->config.x), y=static_cast<uint8_t>(icon->config.y);
  switch (icon->config.type) {
    case UI_ICON_LIGHT: app_light_icon_draw_state(u,x,y,icon->visual_state); break;
    case UI_ICON_LOCK: app_lock_icon_draw_state(u,x,y,icon->visual_state); break;
    case UI_ICON_FAST: app_fast_icon_draw_state(u,x,y,icon->visual_state); break;
    case UI_ICON_WIPER: app_wiper_icon_draw_state(u,x,y,icon->visual_state); break;
    case UI_ICON_HAND_WHEEL: app_hand_wheel_icon_draw_state(u,x,y,icon->visual_state); break;
    case UI_ICON_BATTERY: app_battery_icon_draw_state(u,x,y,icon->visual_state); break;
    default: return false;
  }
  return true;
}

bool ui_icon_draw(ui_icon_t *icon, u8g2_t *u, uint64_t now_us)
{ return u && ui_icon_update(icon,now_us) && ui_icon_render(icon,u); }

bool ui_icon_set_position(ui_icon_t *icon, uint16_t x, uint16_t y)
{
  if (!icon || !icon->initialized || !position_valid(x,y)) return false;
  icon->config.x=x; icon->config.y=y; return true;
}

bool ui_icon_set_period(ui_icon_t *icon, uint32_t period_ms, uint64_t now_us)
{
  if (!icon || !icon->initialized || period_ms == 0U || now_us < icon->last_seen_us) return false;
  if (!ui_icon_update(icon,now_us)) return false;
  const uint64_t phase=phase_at(icon,now_us);
  const uint64_t previous=icon->config.period_ms;
  // Compute phase*period_ms/previous without overflowing for uint32 periods.
  icon->phase_us=(phase/previous)*period_ms + ((phase%previous)*period_ms)/previous;
  icon->anchor_us=icon->last_seen_us=now_us;
  icon->config.period_ms=period_ms;
  return true;
}

bool ui_icon_set_visible(ui_icon_t *icon, bool visible)
{
  if (!icon || !icon->initialized) return false;
  icon->config.visible=visible; return true;
}
