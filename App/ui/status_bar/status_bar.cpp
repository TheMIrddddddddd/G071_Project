#include "status_bar.h"

namespace {
/* Absolute coordinates. Change a row to position, hide, or adjust the complete
 * loop of that icon. phase_ms selects a starting point inside its loop. */
constexpr ui_icon_config_t defaults[] = {
  { UI_ICON_LIGHT,        0U, 0U, 2760U, true, 0U },
  { UI_ICON_LOCK,        21U, 0U, 1960U, true, 0U },
  { UI_ICON_FAST,        42U, 0U, 2520U, true, 0U },
  { UI_ICON_WIPER,       63U, 0U, 6400U, true, 0U },
  { UI_ICON_HAND_WHEEL,  84U, 0U, 3200U, true, 0U },
  { UI_ICON_BATTERY,    105U, 0U, 6640U, true, 0U },
};
constexpr unsigned count=sizeof(defaults)/sizeof(defaults[0]);
static_assert(count == UI_ICON_TYPE_COUNT, "Expected one default for each icon type");
ui_icon_t icons[count];
}

bool status_bar_init(uint64_t now_us)
{
  for (unsigned i=0; i<count; ++i)
    if (!ui_icon_init(&icons[i],&defaults[i],now_us)) return false;
  return true;
}

bool status_bar_update(uint64_t now_us)
{
  /* Reject a stale sample before changing any instance. */
  for (unsigned i=0; i<count; ++i)
    if (!icons[i].initialized || now_us < icons[i].last_seen_us) return false;
  for (unsigned i=0; i<count; ++i)
    if (!ui_icon_update(&icons[i],now_us)) return false;
  return true;
}

bool status_bar_draw(u8g2_t *u)
{
  for (unsigned i=0; i<count; ++i)
    if (!ui_icon_render(&icons[i],u)) return false;
  return true;
}

ui_icon_t *status_bar_get(ui_icon_type_t type)
{
  for (unsigned i=0; i<count; ++i)
    if (icons[i].initialized && icons[i].config.type == type) return &icons[i];
  return nullptr;
}
