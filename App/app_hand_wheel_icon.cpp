#include "app_hand_wheel_icon.h"

namespace {
#include "app_hand_wheel_icon_data.inc"
}

void app_hand_wheel_icon_draw(u8g2_t *u, uint8_t x, uint8_t y, uint64_t elapsed_us)
{
  const auto step = static_cast<unsigned>((elapsed_us / hand_wheel_frame_us) %
                                         sizeof(hand_wheel_sequence));
  u8g2_DrawXBMP(u, x, y, 20U, 16U, hand_wheel_frames[hand_wheel_sequence[step]]);
}
