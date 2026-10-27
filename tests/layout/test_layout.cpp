#include "app_ui.h"
#include "app_joystick.h"
#include "oled_refresh.h"
#include "ui/status_bar/status_bar.h"
#include "ui/components/icons/app_light_icon.h"
#include "u8g2_port.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
static uint8_t frame[1024],off[1024],on[1024];
static const uint64_t epoch=UINT64_C(0x100000000)+123;
static void draw(uint64_t elapsed,bool reset=true) {
 // Icon snapshots use arbitrary timestamps; physics itself always sees monotonic time.
 if(reset)assert(app_ui_init(epoch,frame));
 assert(app_ui_update(epoch+elapsed));
 const app_ui_status_t status={};
 assert(app_ui_draw(frame,&status));
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<128;x++) {
  bool lit=(frame[y/8*128+x]&(1U<<(y%8)))!=0;
  if(y==16)assert(lit);
  else if(y<16&&(x==20||x==41||x==62||x==83||x==104||x>=125))assert(!lit);
  else if(y>16) {
   const bool dual=y>=24&&y<=58&&((x>=6&&x<=40)||(x>=87&&x<=121));
   const bool single=y>=25&&y<=57&&((x>=53&&x<=61)||(x>=66&&x<=74));
   if(!dual&&!single)assert(!lit);
  }
 }
}
static bool region_equal(const uint8_t *a,const uint8_t *b,unsigned x,unsigned width) {
 for(unsigned p=0;p<2;p++)if(std::memcmp(a+p*128+x,b+p*128+x,width))return false;
 return true;
}
static bool pixel(const uint8_t *a,unsigned x,unsigned y) {
 return (a[y/8*128+x]&(1U<<(y%8)))!=0;
}
int main(int argc,char **argv) {
 assert(app_ui_init(epoch,frame));
 draw(0);std::memcpy(off,frame,1024);
 assert(pixel(frame,6,41)&&pixel(frame,40,41)&&pixel(frame,87,41)&&pixel(frame,121,41));
 for(unsigned x=53;x<=61;x++)assert(pixel(frame,x,41));
 for(unsigned x=66;x<=74;x++)assert(pixel(frame,x,41));
 unsigned lit=0;
 for(unsigned x=0;x<20;x++) for(unsigned y=0;y<16;y++)
  if(frame[y/8*128+x]&(1U<<(y%8)))lit++;
 assert(lit>20); // Real off icon, not only the divider.
 unsigned lock_lit=0;
 for(unsigned x=21;x<41;x++)for(unsigned y=0;y<16;y++)if(pixel(frame,x,y))lock_lit++;
 assert(lock_lit>20); // Second status slot must contain the lock.
 unsigned fast_lit=0;
 for(unsigned x=42;x<62;x++)for(unsigned y=0;y<16;y++)if(pixel(frame,x,y))fast_lit++;
 assert(fast_lit>15); // Third status slot contains the crossed-out lightning.
 unsigned hand_lit=0;
 for(unsigned x=84;x<104;x++)for(unsigned y=0;y<16;y++)if(pixel(frame,x,y))hand_lit++;
 assert(hand_lit>20); // Fifth status slot must contain the upward-pointing hand.
 unsigned battery_lit=0;
 for(unsigned x=105;x<125;x++)for(unsigned y=0;y<16;y++)if(pixel(frame,x,y))battery_lit++;
 assert(battery_lit>40); // Sixth status slot must contain the charging battery.
 draw(1080000);std::memcpy(on,frame,1024);assert(!region_equal(on,off,0,20));
 draw(200000);assert(region_equal(frame,off,0,20));
 draw(1500000);assert(region_equal(frame,on,0,20));
 draw(2500000);assert(region_equal(frame,off,0,20));
 draw(2760000);assert(region_equal(frame,off,0,20));
 draw(UINT64_C(2760000)*2000000+1080000);assert(region_equal(frame,on,0,20));
 OLED_RefreshPlan plan;
 assert(OLED_RefreshBuildPlan(off,off,true,OLED_REFRESH_DIFF,&plan)==OLED_REFRESH_NO_CHANGE);
 assert(OLED_RefreshBuildPlan(on,off,true,OLED_REFRESH_DIFF,&plan)==OLED_REFRESH_OK);
 assert(!plan.full&&plan.image_bytes<=250&&plan.wire_bytes<350);
 uint8_t locked[1024],unlocked[1024];
 draw(0);std::memcpy(locked,frame,1024);
 draw(680000);std::memcpy(unlocked,frame,1024);
 assert(!region_equal(locked,unlocked,21,20));
 assert(pixel(locked,36,5)&&!pixel(locked,24,5));
 assert(pixel(unlocked,24,5)&&!pixel(unlocked,36,5));
 draw(460000);assert(pixel(frame,30,1)&&!pixel(frame,36,3)); // Edge-on shackle.
 draw(200000);assert(region_equal(frame,locked,21,20));
 draw(1000000);assert(region_equal(frame,unlocked,21,20));
 draw(1640000);assert(region_equal(frame,locked,21,20));
 draw(1960000);assert(region_equal(frame,locked,21,20));
 draw(UINT64_C(1960000)*2000000+680000);assert(region_equal(frame,unlocked,21,20));
 for(uint64_t ms=0;ms<1960;ms+=40) {
  draw(ms*1000);
  for(unsigned x=21;x<41;x++)for(unsigned y=7;y<16;y++)
   assert(pixel(frame,x,y)==pixel(locked,x,y)); // Body/keyhole never move.
 }
 uint8_t fast_off[1024],fast_on[1024];
 draw(0);std::memcpy(fast_off,frame,1024);
 draw(1200000);std::memcpy(fast_on,frame,1024);
 assert(!region_equal(fast_off,fast_on,42,20));
 draw(400000);assert(region_equal(frame,fast_off,42,20));
 draw(1800000);assert(region_equal(frame,fast_on,42,20));
 draw(2480000);assert(region_equal(frame,fast_off,42,20));
 draw(2520000);assert(region_equal(frame,fast_off,42,20));
 draw(UINT64_C(2520000)*2000000+1200000);assert(region_equal(frame,fast_on,42,20));
 unsigned fast_changes=0;uint8_t prior_fast[1024];std::memcpy(prior_fast,fast_off,1024);
 for(uint64_t ms=0;ms<2520;ms+=20) {
  draw(ms*1000);
  if(!region_equal(frame,prior_fast,42,20))fast_changes++;
  std::memcpy(prior_fast,frame,1024);
 }
 assert(fast_changes>=8);
 uint8_t hand[1024],wheel[1024];
 draw(0);std::memcpy(hand,frame,1024);
 draw(1400000);std::memcpy(wheel,frame,1024);
 assert(!region_equal(hand,wheel,84,20));
 draw(760000);assert(region_equal(frame,hand,84,20));
 draw(2360000);assert(region_equal(frame,wheel,84,20));
 draw(3000000);assert(region_equal(frame,hand,84,20));
 draw(3200000);assert(region_equal(frame,hand,84,20));
 draw(UINT64_C(3200000)*2000000+1400000);assert(region_equal(frame,wheel,84,20));
 // The approved 7x7 star has four equal tips and a clear gap above the right spoke.
 for(unsigned y=0;y<7;y++)for(unsigned x=11;x<18;x++) {
  const bool star=x==14||y==3||(x>=13&&x<=15&&y>=2&&y<=4);
  // x=11 also contains the neighboring wheel rim above/below the left tip.
  if(x>11||star)assert(pixel(wheel,84+x,y)==star);
 }
 for(unsigned x=12;x<18;x++)assert(!pixel(wheel,84+x,7));
 unsigned outward_changes=0,return_changes=0;
 uint8_t prior_hand_wheel[1024];std::memcpy(prior_hand_wheel,hand,1024);
 for(uint64_t ms=0;ms<3200;ms+=40) {
  draw(ms*1000);
  if(!region_equal(frame,prior_hand_wheel,84,20)) {
   if(ms>=800&&ms<=1400)outward_changes++;
   else if(ms>=2400&&ms<=3000)return_changes++;
   else assert(false); // Static holds must not animate the fifth slot.
  }
  std::memcpy(prior_hand_wheel,frame,1024);
 }
 assert(outward_changes>=8&&return_changes>=8);
 uint8_t parked[1024],upright[1024],right[1024];
 draw(0);std::memcpy(parked,frame,1024);
 draw(1100000);std::memcpy(upright,frame,1024);
 draw(1400000);std::memcpy(right,frame,1024);
 assert(!region_equal(parked,upright,63,20));
 assert(!region_equal(parked,right,63,20));
 assert(!region_equal(upright,right,63,20));
 draw(400000);assert(region_equal(frame,parked,63,20));
 draw(2300000);assert(region_equal(frame,upright,63,20));
 draw(5600000);assert(region_equal(frame,parked,63,20));
 draw(6360000);assert(region_equal(frame,parked,63,20));
 draw(6400000);assert(region_equal(frame,parked,63,20));
 draw(UINT64_C(6400000)*2000000+1100000);assert(region_equal(frame,upright,63,20));
 unsigned wiper_changes=0;uint8_t prior_wiper[1024];std::memcpy(prior_wiper,parked,1024);
 for(uint64_t ms=0;ms<6400;ms+=40) {
  draw(ms*1000);
  if(ms<800||ms>=5600)assert(region_equal(frame,parked,63,20));
  if(!region_equal(frame,prior_wiper,63,20))wiper_changes++;
  std::memcpy(prior_wiper,frame,1024);
 }
 assert(wiper_changes>=20);
 uint8_t charging[1024],levels[4][1024];
 draw(0);std::memcpy(charging,frame,1024);
 draw(1600000);assert(region_equal(frame,charging,105,20));
 const uint64_t level_times[]={2200000,3200000,4200000,5200000};
 for(unsigned level=0;level<4;level++) {
  draw(level_times[level]);std::memcpy(levels[level],frame,1024);
  assert(!region_equal(frame,charging,105,20));
  // Four separated bars: occupied cells are solid; unused cells and gaps are black.
  for(unsigned bar=0;bar<4;bar++)for(unsigned y=4;y<12;y++) {
   for(unsigned dx=0;dx<2;dx++)assert(pixel(frame,105+3+bar*3+dx,y)==(bar<=level));
   assert(!pixel(frame,105+5+bar*3,y));
  }
  draw(level_times[level]+400000);assert(region_equal(frame,levels[level],105,20));
 }
 draw(6600000);assert(region_equal(frame,charging,105,20));
 draw(6640000);assert(region_equal(frame,charging,105,20));
 draw(UINT64_C(6640000)*2000000+4200000);assert(region_equal(frame,levels[2],105,20));
 uint8_t prior_battery[1024];std::memcpy(prior_battery,charging,1024);
 unsigned battery_changes=0;
 for(uint64_t ms=0;ms<6640;ms+=40) {
  draw(ms*1000);
  if(!region_equal(frame,prior_battery,105,20))battery_changes++;
  std::memcpy(prior_battery,frame,1024);
  // The shell and terminal remain fixed through every transition.
  for(unsigned x=1;x<16;x++)assert(pixel(frame,105+x,2)&&pixel(frame,105+x,13));
  for(unsigned y=3;y<13;y++)assert(pixel(frame,105,y)&&pixel(frame,121,y));
  for(unsigned y=6;y<10;y++)assert(pixel(frame,122,y)&&pixel(frame,123,y));
 }
 assert(battery_changes>=20);
 uint8_t previous[1024];std::memcpy(previous,off,1024);unsigned changed=0;
 FILE *output=(argc==2)?std::fopen(argv[1],"wb"):nullptr;
 if(argc==2)assert(output);
 assert(app_ui_init(epoch,frame));
 for(uint64_t ms=0;ms<12800;ms+=40) {
  draw(ms*1000,false);
  if(std::memcmp(previous,frame,1024)!=0)changed++;
  std::memcpy(previous,frame,1024);
  if(output)assert(std::fwrite(frame,1,1024,output)==1024);
 }
 if(output)std::fclose(output);
 assert(changed>=8);
 // Holding all joystick inputs at zero must not freeze the independently timed status bar.
 assert(app_ui_init(epoch,frame));const app_joystick_targets_t neutral{};
 assert(app_joystick_set_targets(epoch,&neutral));
 draw(0,false);std::memcpy(previous,frame,1024);
 draw(1400000,false);
 assert(std::memcmp(previous,frame,256)!=0);
 assert(std::memcmp(previous+256,frame+256,768)==0);
 assert(!app_ui_failed()&&app_ui_heap_peak()==0);
  // Configure the actual page component, then verify it renders at the new
  // position and speed through the normal app_ui update/draw path.
  assert(app_ui_init(epoch,frame));
  for(unsigned k=0;k<UI_ICON_TYPE_COUNT;k++)
    assert(ui_icon_set_visible(status_bar_get(static_cast<ui_icon_type_t>(k)),false));
  ui_icon_t *light=status_bar_get(UI_ICON_LIGHT);
  assert(light&&ui_icon_set_position(light,60,0));
  assert(ui_icon_set_period(light,5520,epoch));
  assert(ui_icon_set_visible(light,true));
  assert(app_ui_update(epoch+1200000));
  const app_ui_status_t status={};
  assert(app_ui_draw(frame,&status));
  U8G2_PortBind(on);u8g2_ClearBuffer(U8G2_PortContext());
  app_light_icon_draw(U8G2_PortContext(),60,0,600000);
  assert(region_equal(frame,on,60,20));
 std::printf("PASS: six status icons bounds/holds/independent cycles/lock/star/wiper/battery charging and four levels/64-bit time/diff; %u changed frames\n",changed);
}
