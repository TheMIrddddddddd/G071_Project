#include "joystick_follow.h"
#include "core/animation/generators/spring/spring.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace {
JoystickSnapshot state{};
// Direct generators avoid Animate's heap allocation and millisecond HAL clock.
// All calls belong to the existing single foreground context, never an ISR.
smooth_ui_toolkit::Spring springs[6];
uint64_t segment_start_us[6];
bool initialized, have_time;
bool settled() {
  for (const auto &spring : springs)
    if (!spring.done) return false;
  return true;
}
bool valid_stiffness(float value) { return std::isfinite(value) && value > 0; }
bool finite(JoystickPoint p) {
  return std::isfinite(p.x) && std::isfinite(p.y);
}
JoystickPoint clamp_radial(JoystickPoint p) {
  // Scale first to avoid overflow for finite but extremely large inputs.
  float largest = std::max(std::abs(p.x), std::abs(p.y));
  if (largest == 0)
    return p;
  float x = p.x / largest, y = p.y / largest;
  float unit_length = std::sqrt(x * x + y * y);
  float scale = JOYSTICK_MAX_OFFSET_PX / unit_length;
  if (largest > scale)
    return {x * scale, y * scale};
  return p;
}
void retarget(unsigned index, float value) {
  auto &spring = springs[index];
  if (value == spring.end) return; // Repeated ADC targets must not restart a segment.
  spring.retarget(spring.value, value); // Library preserves sampled velocity.
  segment_start_us[index] = state.sample_us;
}
void publish_current() {
  // Inertia during retarget may leave the geometric domain. Clip the output,
  // while preserving the underlying spring position/velocity for continuity.
  state.left_current = springs[0].done && springs[1].done ? state.left_target :
      clamp_radial({springs[0].value, springs[1].value});
  state.right_current = springs[2].done && springs[3].done ? state.right_target :
      clamp_radial({springs[2].value, springs[3].value});
  state.single_current = {
      std::clamp(springs[4].value, -JOYSTICK_MAX_OFFSET_PX, JOYSTICK_MAX_OFFSET_PX),
      std::clamp(springs[5].value, -JOYSTICK_MAX_OFFSET_PX, JOYSTICK_MAX_OFFSET_PX)};
  state.settled = settled();
}
void quantize_offset(JoystickPoint point, int16_t &x, int16_t &y) {
  point = clamp_radial(point);
  float ax = std::abs(point.x), ay = std::abs(point.y);
  int fx = static_cast<int>(std::floor(ax)),
      fy = static_cast<int>(std::floor(ay));
  float best = std::numeric_limits<float>::max();
  int bx = 0, by = 0;
  // Search four nearby nonnegative lattice points. The same tie rule in
  // absolute space guarantees Q(-x,y)=(-Qx,Qy), including half-pixel ties.
  for (int dx = 0; dx < 2; ++dx)
    for (int dy = 0; dy < 2; ++dy) {
      int cx = fx + dx, cy = fy + dy;
      if (cx * cx + cy * cy > JOYSTICK_MAX_OFFSET_PX * JOYSTICK_MAX_OFFSET_PX)
        continue;
      float distance = (cx - ax) * (cx - ax) + (cy - ay) * (cy - ay);
      if (distance < best) {
        best = distance;
        bx = cx;
        by = cy;
      }
    }
  x = static_cast<int16_t>(point.x < 0 ? -bx : bx);
  y = static_cast<int16_t>(point.y < 0 ? -by : by);
}
} // namespace
bool Joystick_Init(float stiffness) {
  if (!valid_stiffness(stiffness))
    return false;
  state = {};
  state.settled = 1;
  for (unsigned i = 0; i < 6; ++i) {
    auto &spring = springs[i];
    spring.start = spring.end = 0;
    spring.springOptions = {};
    spring.springOptions.stiffness = stiffness;
    spring.springOptions.mass = JOYSTICK_SPRING_MASS;
    spring.springOptions.damping = 2 * std::sqrt(stiffness * JOYSTICK_SPRING_MASS);
    spring.springOptions.restDelta = JOYSTICK_SNAP_PX;
    spring.springOptions.restSpeed = JOYSTICK_SNAP_PX;
    spring.init();
    spring.next(0);
    segment_start_us[i] = 0;
  }
  initialized = true;
  have_time = false;
  return true;
}
bool Joystick_SetStiffness(float stiffness) {
  if (!initialized || !valid_stiffness(stiffness))
    return false;
  for (unsigned i = 0; i < 6; ++i) {
    auto &spring = springs[i];
    if (spring.springOptions.stiffness == stiffness) continue;
    spring.springOptions.stiffness = stiffness;
    spring.springOptions.damping = 2 * std::sqrt(stiffness * JOYSTICK_SPRING_MASS);
    spring.retarget(spring.value, spring.end);
    spring.next(0);
    segment_start_us[i] = state.sample_us;
  }
  state.settled = settled();
  return true;
}
bool Joystick_SetTargets(float lx, float ly, float rx, float ry) {
  JoystickPoint left{lx, ly}, right{rx, ry};
  if (!initialized || !finite(left) || !finite(right))
    return false;
  state.left_target = clamp_radial(left);
  state.right_target = clamp_radial(right);
  retarget(0, state.left_target.x); retarget(1, state.left_target.y);
  retarget(2, state.right_target.x); retarget(3, state.right_target.y);
  state.settled = settled();
  return true;
}
bool Joystick_SetSingleTargets(float joy3, float joy4) {
  if (!initialized || !std::isfinite(joy3) || !std::isfinite(joy4)) return false;
  state.single_target = {std::clamp(joy3,-JOYSTICK_MAX_OFFSET_PX,JOYSTICK_MAX_OFFSET_PX),
                         std::clamp(joy4,-JOYSTICK_MAX_OFFSET_PX,JOYSTICK_MAX_OFFSET_PX)};
  retarget(4, state.single_target.x); retarget(5, state.single_target.y);
  state.settled = settled();
  return true;
}
bool Joystick_Update(uint64_t now_us) {
  if (!initialized || (have_time && now_us < state.sample_us))
    return false;
  if (!have_time) {
    state.sample_us = now_us;
    for (auto &start : segment_start_us) start = now_us;
    have_time = true;
    return true;
  }
  uint64_t elapsed = now_us - state.sample_us;
  if (elapsed == 0)
    return true;
  state.sample_us = now_us;
  if (!state.settled) {
    for (unsigned i = 0; i < 6; ++i) {
      if (!springs[i].done) {
        // Subtract uint64 timestamps before conversion. Long absolute uptime
        // never loses a short step, and no SmoothPort 60-second guard is used.
        const float seconds = static_cast<float>(now_us - segment_start_us[i]) * 1e-6f;
        springs[i].next(seconds);
      }
    }
    publish_current();
  }
  return true;
}
bool Joystick_GetSnapshot(JoystickSnapshot *out) {
  if (!initialized || !out)
    return false;
  *out = state;
  return true;
}
bool Joystick_Quantize(const JoystickSnapshot *s, JoystickPixels *p) {
  if (!s || !p || !finite(s->left_current) || !finite(s->right_current))
    return false;
  int16_t lx, ly, rx, ry;
  quantize_offset(s->left_current, lx, ly);
  quantize_offset(s->right_current, rx, ry);
  *p = {static_cast<int16_t>(32 + lx), static_cast<int16_t>(32 + ly),
        static_cast<int16_t>(95 + rx), static_cast<int16_t>(32 + ry)};
  return true;
}
bool Joystick_SetAutoTargets(uint64_t elapsed_us) {
  struct Target {
    uint32_t us;
    float x, y;
  };
  static const Target schedule[] = {
      {0, 0, 0},         {800000, 16, 0},         {1120000, -16, 0},
      {2150000, 0, -16}, {3250000, 11.3f, 11.3f}, {4350000, -11.3f, -11.3f},
      {5450000, 16, 0},  {6550000, 0, 16},        {7650000, -16, 0},
      {8800000, 0, 0}};
  uint32_t phase = static_cast<uint32_t>(elapsed_us % JOYSTICK_PERIOD_US);
  unsigned index = 0;
  while (index + 1U < sizeof(schedule) / sizeof(schedule[0]) &&
         schedule[index + 1U].us <= phase)
    ++index;
  return Joystick_SetTargets(schedule[index].x, schedule[index].y,
                             -schedule[index].x, schedule[index].y);
}
