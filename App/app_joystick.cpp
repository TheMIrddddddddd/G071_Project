#include "app_joystick.h"
#include "joystick_follow.h"
#include "app_joystick_demo.h"
#include "app_joystick_view.h"
#include "app_joystick_geometry.hpp"
#include <algorithm>
#include <cmath>

namespace {
constexpr uint64_t sample_period_us=40000U;
using joystick_geometry::dual_travel;
using joystick_geometry::single_travel;
using joystick_geometry::radial;
uint64_t epoch, last_seen, last_sample;
bool ready, demo;

float unit(float value) { return std::clamp(value,-1.0f,1.0f); }
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
  return !demo||apply(app_joystick_demo_targets(now_us-epoch));
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
  app_joystick_view_draw(u,state);
  return true;
}
