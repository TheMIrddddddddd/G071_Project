#include "ui/components/animated_icon.h"
#include "ui/status_bar/status_bar.h"
#include "u8g2_port.h"
#include "app_light_icon.h"
#include "app_lock_icon.h"
#include "app_fast_icon.h"
#include "app_wiper_icon.h"
#include "app_hand_wheel_icon.h"
#include "app_battery_icon.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint8_t actual[1024], expected[1024];
using Draw = void (*)(u8g2_t *, uint8_t, uint8_t, uint64_t);
static const Draw original[] = { app_light_icon_draw, app_lock_icon_draw,
  app_fast_icon_draw, app_wiper_icon_draw, app_hand_wheel_icon_draw,
  app_battery_icon_draw };
static const uint32_t periods[] = {2760, 1960, 2520, 6400, 3200, 6640};
static u8g2_t *ctx;
static void clear(uint8_t *buffer) { U8G2_PortBind(buffer); u8g2_ClearBuffer(ctx); }
static bool pixel(const uint8_t *buffer, unsigned x, unsigned y)
{ return (buffer[(y / 8) * 128 + x] & (1U << (y % 8))) != 0; }

int main() {
  constexpr uint64_t start = UINT64_C(0x100000000) + 123;
  ctx = U8G2_PortInit(actual); assert(ctx);
  for (unsigned k=0; k<6; ++k) {
    ui_icon_t icon{};
    ui_icon_config_t config{static_cast<ui_icon_type_t>(k), 0, 0, periods[k], true, 0};
    assert(ui_icon_init(&icon, &config, start));
    const uint64_t times[] = {0, 1, 39999, 40000, 999999, 1234567,
      static_cast<uint64_t>(periods[k])*1000-1,
      static_cast<uint64_t>(periods[k])*1000, UINT64_C(8000000000001)};
    for (uint64_t t : times) {
      clear(actual); assert(ui_icon_draw(&icon, ctx, start+t));
      clear(expected); original[k](ctx,0,0,t);
      assert(std::memcmp(actual, expected, sizeof(actual))==0);
    }
    // The same artwork is independently placed, and the 2x period halves phase.
    config.x=80; config.y=40; config.period_ms=periods[k]*2;
    ui_icon_t other{}; assert(ui_icon_init(&other, &config, start));
    clear(actual); assert(ui_icon_draw(&other, ctx, start+1200000));
    clear(expected); original[k](ctx,0,0,600000);
    for (unsigned y=0; y<16; ++y) for (unsigned x=0; x<20; ++x)
      assert(pixel(actual,x+80,y+40)==pixel(expected,x,y));
  }
  ui_icon_t icon{};
  ui_icon_config_t config{UI_ICON_LIGHT,10,10,2760,true,0};
  assert(ui_icon_init(&icon,&config,start));
  clear(actual); assert(ui_icon_draw(&icon,ctx,start+725123));
  assert(ui_icon_set_period(&icon,5520,start+725123));
  clear(expected); assert(ui_icon_draw(&icon,ctx,start+725123));
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  clear(actual); assert(ui_icon_draw(&icon,ctx,start+925123));
  clear(expected); original[0](ctx,10,10,825123);
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  assert(!ui_icon_set_period(&icon,0,start+925123));
  assert(!ui_icon_set_period(&icon,10,start));
  assert(!ui_icon_set_position(&icon,109,0));
  assert(!ui_icon_set_position(&icon,0,49));
  assert(ui_icon_set_position(&icon,108,48));
  assert(ui_icon_set_visible(&icon,false));
  clear(actual); assert(ui_icon_draw(&icon,ctx,start+1000000));
  clear(expected); assert(!std::memcmp(actual,expected,sizeof(actual)));
  assert(ui_icon_set_visible(&icon,true));
  clear(actual); assert(ui_icon_draw(&icon,ctx,start+1125123));
  clear(expected); original[0](ctx,108,48,925123);
  assert(!std::memcmp(actual,expected,sizeof(actual))); // Hidden clock continued.
  assert(!ui_icon_draw(&icon,ctx,start));
  config.period_ms=0; assert(!ui_icon_init(&icon,&config,start));
  config.period_ms=2760; config.x=109; assert(!ui_icon_init(&icon,&config,start));
  assert(status_bar_init(start));
  for (unsigned k=0; k<6; ++k) assert(status_bar_get(static_cast<ui_icon_type_t>(k)));
  assert(!status_bar_get(static_cast<ui_icon_type_t>(6)));
  assert(status_bar_update(start+1234567));
  clear(actual); assert(status_bar_draw(ctx));
  clear(expected); for (unsigned k=0;k<6;++k) original[k](ctx,static_cast<uint8_t>(k*21),0,1234567);
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  // A sample is stable across draws; advancing time is a separate operation.
  ui_icon_t sampled{}; ui_icon_config_t sample_config{UI_ICON_LIGHT,0,0,2760,true,0};
  assert(ui_icon_init(&sampled,&sample_config,start));
  assert(ui_icon_update(&sampled,start+1080000));
  clear(actual); assert(ui_icon_render(&sampled,ctx));
  clear(expected); assert(ui_icon_render(&sampled,ctx));
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  assert(ui_icon_update(&sampled,start+2500000));
  clear(expected); assert(ui_icon_render(&sampled,ctx));
  assert(std::memcmp(actual,expected,sizeof(actual)) != 0);
  // Two instances of the same icon keep independent location, phase and period.
  ui_icon_t first{}, second{};
  ui_icon_config_t a{UI_ICON_BATTERY,0,0,6640,true,0};
  ui_icon_config_t b{UI_ICON_BATTERY,60,20,3320,true,800};
  assert(ui_icon_init(&first,&a,start) && ui_icon_init(&second,&b,start));
  clear(actual);
  assert(ui_icon_draw(&first,ctx,start+1000000));
  assert(ui_icon_draw(&second,ctx,start+1000000));
  clear(expected);
  original[5](ctx,0,0,1000000);
  original[5](ctx,60,20,3600000);
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  // The largest supported uint32 period remains usable across live changes.
  b.period_ms=UINT32_MAX; b.phase_ms=UINT32_MAX-1;
  assert(ui_icon_init(&second,&b,start));
  clear(actual); assert(ui_icon_draw(&second,ctx,start+500));
  assert(ui_icon_set_period(&second,UINT32_MAX-1,start+500));
  clear(expected); assert(ui_icon_draw(&second,ctx,start+500));
  assert(!std::memcmp(actual,expected,sizeof(actual)));
  puts("PASS: independent icons, default pixels, placement, period, live reconfiguration and bounds");
}
