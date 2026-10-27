#include "app_joystick_view.h"
#include "app_joystick_geometry.hpp"
#include <algorithm>
#include <cmath>

namespace {
using joystick_geometry::dual_travel;
using joystick_geometry::single_travel;
using joystick_geometry::radial;
constexpr int center_y=41, ring_radius=17, dot_radius=2;
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

void app_joystick_view_draw(u8g2_t *u, const JoystickSnapshot &state) {
  u8g2_SetDrawColor(u,1);
  dual(u,23,state.left_current);dual(u,104,state.right_current);
  single(u,57,state.single_current.x);single(u,70,state.single_current.y);
}
