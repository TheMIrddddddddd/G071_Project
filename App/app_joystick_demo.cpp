#include "app_joystick_demo.h"
#include <cstddef>

namespace {
struct Key { uint16_t ms; float x,y; };
template<std::size_t N> const Key &at(const Key (&keys)[N],uint32_t ms) {
  std::size_t i=0;
  while(i+1<N&&keys[i+1].ms<=ms)++i;
  return keys[i];
}
} // namespace

app_joystick_targets_t app_joystick_demo_targets(uint64_t elapsed) {
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
  return {l.x,l.y,r.x,r.y,a.x,b.x};
}
