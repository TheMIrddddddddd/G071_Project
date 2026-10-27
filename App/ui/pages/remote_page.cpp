#include "remote_page.h"
#include "app_joystick.h"
#include "status_bar.h"

namespace {
uint64_t page_now;
}

bool remote_page_init(uint64_t now_us)
{
  page_now = now_us;
  return status_bar_init(now_us) && app_joystick_init(now_us);
}

bool remote_page_update(uint64_t now_us)
{
  if (!app_joystick_update(now_us)) return false;
  page_now = now_us;
  return true;
}

bool remote_page_draw(u8g2_t *display)
{
  if (display == nullptr) return false;
  if (!status_bar_update(page_now)) return false;
  u8g2_DrawHLine(display, 0, 16, 128);
  if (!status_bar_draw(display)) return false;
  return app_joystick_draw(display);
}
