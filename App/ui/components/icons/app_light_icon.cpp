#include "app_light_icon.h"

namespace {
/* Original 20x16 artwork based on the user's physical switch: sideways lamp,
   left socket, off slash, and on-state inner arc plus four rays. Bit 0 is x=0. */
constexpr uint32_t off_rows[16] = {
  0x00000U,
  0x60000U,
  0x30000U,
  0x19E00U,
  0x0C180U,
  0x0607AU,
  0x1300AU,
  0x11C0AU,
  0x1060AU,
  0x1030AU,
  0x081BAU,
  0x040C0U,
  0x03E60U,
  0x00030U,
  0x00018U,
  0x00000U
};
constexpr uint32_t on_rows[16] = {
  0x00800U,
  0x40800U,
  0x20000U,
  0x01E00U,
  0x06180U,
  0x0807AU,
  0x1000AU,
  0xD400AU,
  0x1400AU,
  0x1300AU,
  0x0887AU,
  0x04180U,
  0x23E00U,
  0x40000U,
  0x00000U,
  0x00000U
};
/* Ordered dissolve keeps this monochrome animation stable between updates. */
constexpr uint8_t threshold[4][4] = {
  {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5}
};
uint8_t ease(uint32_t ms)
{
  const uint32_t t=ms*256U/600U;
  return static_cast<uint8_t>((t*t*(768U-2U*t))>>20U); // Smoothstep, 0..16.
}
uint8_t on_level(uint64_t elapsed_us)
{
  // Source GIF: 480ms OFF, 600ms transition, 640ms ON, 600ms transition,
  // 440ms OFF. Each 40ms render samples the same 2760ms repeating timeline.
  const uint32_t ms=static_cast<uint32_t>((elapsed_us/1000U)%2760U);
  if(ms<480U)return 0;
  if(ms<1080U)return ease(ms-480U);
  if(ms<1720U)return 16;
  if(ms<2320U)return static_cast<uint8_t>(16U-ease(ms-1720U));
  return 0;
}
}
int16_t app_light_icon_sample(uint64_t elapsed_us) { return on_level(elapsed_us); }
void app_light_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state)
{
  const uint8_t level=static_cast<uint8_t>(state);
  for(uint8_t row=0;row<16U;row++)for(uint8_t col=0;col<20U;col++) {
    const uint32_t bits=(level>threshold[row&3U][col&3U])?on_rows[row]:off_rows[row];
    if(bits&(1UL<<col))u8g2_DrawPixel(u,x+col,y+row);
  }
}
void app_light_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us)
{ app_light_icon_draw_state(u,x,y,app_light_icon_sample(elapsed_us)); }
