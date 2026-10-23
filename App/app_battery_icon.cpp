#include "app_battery_icon.h"

namespace {
#include "app_battery_icon_data.inc"
}

void app_battery_icon_draw(u8g2_t *u, uint8_t x, uint8_t y, uint64_t elapsed_us)
{
  const uint32_t ms = static_cast<uint32_t>((elapsed_us / 1000U) % battery_period_ms);
  for (const auto &step : battery_steps) {
    if (ms < step.end_ms) {
      u8g2_DrawXBMP(u, x, y, 20U, 16U, battery_frames[step.frame]);
      return;
    }
  }
}
