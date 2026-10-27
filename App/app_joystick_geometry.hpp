#ifndef APP_JOYSTICK_GEOMETRY_HPP
#define APP_JOYSTICK_GEOMETRY_HPP
#include "joystick_follow.h"
#include <cmath>

namespace joystick_geometry {
constexpr float dual_travel=10.0f, single_travel=11.0f;
inline JoystickPoint radial(JoystickPoint point,float limit) {
  const float squared=point.x*point.x+point.y*point.y;
  if(squared>limit*limit) {
    const float scale=limit/std::sqrt(squared);
    point.x*=scale;point.y*=scale;
  }
  return point;
}
} // namespace joystick_geometry
#endif
