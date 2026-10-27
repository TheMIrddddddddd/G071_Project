#include "app_fast_icon.h"

namespace {
#include "app_fast_icon_data.inc"
}

int16_t app_fast_icon_sample(uint64_t elapsed_us)
{
  const uint32_t ms=static_cast<uint32_t>((elapsed_us/1000U)%fast_period_ms);
  for(const auto &step:fast_steps) {
    if(ms<step.end_ms) {
      return step.frame;
    }
  }
  return -1;
}
void app_fast_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state)
{ if(state>=0 && static_cast<unsigned>(state)<sizeof(fast_frames)/sizeof(fast_frames[0]))
    u8g2_DrawXBMP(u,x,y,20U,16U,fast_frames[state]); }
void app_fast_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us)
{ app_fast_icon_draw_state(u,x,y,app_fast_icon_sample(elapsed_us)); }
