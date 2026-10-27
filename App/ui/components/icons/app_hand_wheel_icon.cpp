#include "app_hand_wheel_icon.h"

namespace {
#include "app_hand_wheel_icon_data.inc"
}

int16_t app_hand_wheel_icon_sample(uint64_t elapsed_us)
{
  const auto step = static_cast<unsigned>((elapsed_us / hand_wheel_frame_us) %
                                         sizeof(hand_wheel_sequence));
  return hand_wheel_sequence[step];
}
void app_hand_wheel_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state)
{ if(state>=0 && static_cast<unsigned>(state)<sizeof(hand_wheel_frames)/sizeof(hand_wheel_frames[0]))
    u8g2_DrawXBMP(u,x,y,20U,16U,hand_wheel_frames[state]); }
void app_hand_wheel_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us)
{ app_hand_wheel_icon_draw_state(u,x,y,app_hand_wheel_icon_sample(elapsed_us)); }
