#include "app_lock_icon.h"

namespace {
struct Point { int16_t x,y; };
/* Left leg is the hinge. The free right leg swings to the left of the body. */
constexpr Point shackle[] = {
  {9,6},{9,3},{10,1},{11,0},{13,0},{14,1},{15,3},{15,6}
};
int16_t opening_scale(uint32_t ms)
{
  const uint32_t t=ms*256U/440U;
  const uint32_t eased=(t*t*(768U-2U*t))>>16U;
  return static_cast<int16_t>(256-2*static_cast<int32_t>(eased));
}
int16_t shackle_scale(uint64_t elapsed_us)
{
  // Reference GIF: 240ms closed, 440ms opening, 520ms open,
  // 440ms closing, 320ms closed. Horizontal projection crosses zero at 90deg.
  const uint32_t ms=static_cast<uint32_t>((elapsed_us/1000U)%1960U);
  if(ms<240U)return 256;
  if(ms<680U)return opening_scale(ms-240U);
  if(ms<1200U)return -256;
  if(ms<1640U)return static_cast<int16_t>(-opening_scale(ms-1200U));
  return 256;
}
int16_t projected_x(int16_t x,int16_t scale)
{
  const int32_t product=static_cast<int32_t>(x-9)*scale;
  const int32_t rounded=product>=0?(product+128)/256:-((-product+128)/256);
  return static_cast<int16_t>(9+rounded);
}
}
void app_lock_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us)
{
  const int16_t scale=shackle_scale(elapsed_us);
  for(unsigned i=1;i<sizeof(shackle)/sizeof(shackle[0]);i++) {
    const Point a=shackle[i-1],b=shackle[i];
    u8g2_DrawLine(u,static_cast<u8g2_uint_t>(x+projected_x(a.x,scale)),
                   static_cast<u8g2_uint_t>(y+a.y),
                   static_cast<u8g2_uint_t>(x+projected_x(b.x,scale)),
                   static_cast<u8g2_uint_t>(y+b.y));
  }
  u8g2_DrawRFrame(u,x+7U,y+6U,11U,9U,2U);
  u8g2_DrawVLine(u,x+12U,y+10U,2U);
}
