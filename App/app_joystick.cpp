#include "app_joystick.h"
#include "joystick_follow.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
constexpr uint64_t sample_period_us=40000U;
constexpr int center_y=41, ring_radius=17, dot_radius=2;
constexpr float dual_travel=10.0f, single_travel=11.0f;
uint64_t epoch, last_seen, last_sample;
bool ready, demo;

float unit(float value) { return std::clamp(value,-1.0f,1.0f); }
JoystickPoint radial(JoystickPoint point,float limit) {
  const float squared=point.x*point.x+point.y*point.y;
  if(squared>limit*limit) {
    const float scale=limit/std::sqrt(squared);
    point.x*=scale;point.y*=scale;
  }
  return point;
}
bool valid(const app_joystick_targets_t &t) {
  return std::isfinite(t.left_x)&&std::isfinite(t.left_y)&&
         std::isfinite(t.right_x)&&std::isfinite(t.right_y)&&
         std::isfinite(t.single_left)&&std::isfinite(t.single_right);
}
bool apply(const app_joystick_targets_t &t) {
  const auto left=radial({unit(t.left_x)*dual_travel,-unit(t.left_y)*dual_travel},dual_travel);
  const auto right=radial({unit(t.right_x)*dual_travel,-unit(t.right_y)*dual_travel},dual_travel);
  return Joystick_SetTargets(left.x,left.y,right.x,right.y)&&
         Joystick_SetSingleTargets(unit(t.single_left)*single_travel,unit(t.single_right)*single_travel);
}
struct Key { uint16_t ms; float x,y; };
template<std::size_t N> const Key &at(const Key (&keys)[N],uint32_t ms) {
  std::size_t i=0;
  while(i+1<N&&keys[i+1].ms<=ms)++i;
  return keys[i];
}
bool auto_targets(uint64_t elapsed) {
  static constexpr Key left[]={
    {0,0,0},{800,1,0},{1120,-1,0},{2000,0,1},{2800,.7f,.7f},
    {3600,1,0},{4400,.7f,-.7f},{5200,0,-1},{6000,-1,0},{6800,0,0}};
  static constexpr Key right[]={
    {0,0,0},{1200,0,1},{2000,1,0},{2800,0,-1},{3600,-1,0},
    {4400,.7f,.7f},{5200,-.7f,-.7f},{6000,0,0}};
  static constexpr Key single_left[]={
    {0,0,0},{1200,1,0},{2400,-1,0},{3600,.5f,0},{4400,0,0}};
  static constexpr Key single_right[]={
    {0,0,0},{800,-1,0},{2000,1,0},{3200,-.5f,0},{4400,.5f,0},{5600,0,0}};
  const auto &l=at(left,static_cast<uint32_t>((elapsed/1000U)%8000U));
  const auto &r=at(right,static_cast<uint32_t>((elapsed/1000U)%7200U));
  const auto &a=at(single_left,static_cast<uint32_t>((elapsed/1000U)%5600U));
  const auto &b=at(single_right,static_cast<uint32_t>((elapsed/1000U)%6800U));
  return apply({l.x,l.y,r.x,r.y,a.x,b.x});
}
void quantize(JoystickPoint point,int &x,int &y) {
  point=radial(point,dual_travel);
  const float ax=std::abs(point.x),ay=std::abs(point.y);
  const int fx=static_cast<int>(ax),fy=static_cast<int>(ay);
  float best=1e30f;int bx=0,by=0;
  // Nearest valid lattice point; rounding a diagonal must not expand the travel disk.
  for(int dx=0;dx<2;++dx)for(int dy=0;dy<2;++dy) {
    const int cx=fx+dx,cy=fy+dy;
    if(cx*cx+cy*cy>dual_travel*dual_travel)continue;
    const float distance=(cx-ax)*(cx-ax)+(cy-ay)*(cy-ay);
    if(distance<best){best=distance;bx=cx;by=cy;}
  }
  x=point.x<0?-bx:bx;y=point.y<0?-by:by;
}
void knob(u8g2_t *u,int x,int y) {
  u8g2_SetDrawColor(u,0);
  u8g2_DrawDisc(u,x,y,dot_radius+1,U8G2_DRAW_ALL);
  u8g2_SetDrawColor(u,1);
  u8g2_DrawDisc(u,x,y,dot_radius,U8G2_DRAW_ALL);
}
void dual(u8g2_t *u,int cx,JoystickPoint point) {
  u8g2_DrawCircle(u,cx,center_y,ring_radius,U8G2_DRAW_ALL);
  u8g2_DrawHLine(u,cx-2,center_y,5);
  u8g2_DrawVLine(u,cx,center_y-2,5);
  u8g2_DrawHLine(u,cx-16,center_y,3);
  u8g2_DrawHLine(u,cx+14,center_y,3);
  u8g2_DrawVLine(u,cx,center_y-16,3);
  u8g2_DrawVLine(u,cx,center_y+14,3);
  int x,y;quantize(point,x,y);knob(u,cx+x,center_y+y);
}
void single(u8g2_t *u,int x,float value) {
  u8g2_DrawVLine(u,x,25,33);
  u8g2_DrawHLine(u,x-2,25,5);
  u8g2_DrawHLine(u,x-2,57,5);
  const int offset=static_cast<int>(std::round(std::clamp(value,-single_travel,single_travel)));
  knob(u,x,center_y-offset);
  // Keep the complete symmetric zero line visible even as the knob crosses it.
  u8g2_DrawHLine(u,x-4,center_y,9);
}
} // namespace

bool app_joystick_init(uint64_t now_us) {
  ready=Joystick_Init(JOYSTICK_DEFAULT_STIFFNESS)&&Joystick_Update(now_us);
  epoch=last_seen=last_sample=now_us;demo=true;
  return ready;
}
bool app_joystick_update(uint64_t now_us) {
  if(!ready||now_us<last_seen)return false;
  last_seen=now_us;
  if(now_us-last_sample<sample_period_us)return true;
  if(!Joystick_Update(now_us))return false;
  last_sample=now_us;
  // A late foreground poll applies only the latest target; no catch-up loop.
  return !demo||auto_targets(now_us-epoch);
}
bool app_joystick_set_targets(uint64_t now_us,const app_joystick_targets_t *targets) {
  if(!ready||!targets||!valid(*targets)||now_us<last_seen)return false;
  if(!Joystick_Update(now_us)||!apply(*targets))return false;
  last_seen=last_sample=now_us;demo=false;
  return true;
}
bool app_joystick_draw(u8g2_t *u) {
  JoystickSnapshot state;
  if(!ready||!u||!Joystick_GetSnapshot(&state))return false;
  u8g2_SetDrawColor(u,1);
  dual(u,23,state.left_current);dual(u,104,state.right_current);
  single(u,57,state.single_current.x);single(u,70,state.single_current.y);
  return true;
}
