#include "joystick_draw.h"
#include "u8g2_port.h"
namespace {
bool valid_center(int x, int y, int cx) {
  int dx = x - cx, dy = y - 32;
  // Limit before squaring: caller-supplied int16_t coordinates cannot overflow.
  if (dx < -22 || dx > 22 || dy < -22 || dy > 22)
    return false;
  return dx * dx + dy * dy <= JOYSTICK_MAX_OFFSET_PX * JOYSTICK_MAX_OFFSET_PX;
}
void dot(u8g2_t *u, int cx, int cy) {
  // 9*(dx*dx+dy*dy)<=256; radius=16/3, an 11x11 template with 89 set pixels.
  // Row half widths for dy=-5..5. This is not an integer-radius-6 disc.
  static const uint8_t half_widths[11] = {1, 3, 4, 4, 5, 5, 5, 4, 4, 3, 1};
  for (int row = 0; row < 11; ++row) {
    int half = half_widths[row];
    u8g2_DrawHLine(u, static_cast<u8g2_uint_t>(cx - half),
                   static_cast<u8g2_uint_t>(cy + row - 5),
                   static_cast<u8g2_uint_t>(2 * half + 1));
  }
}
} // namespace
bool Joystick_DrawFrame(uint8_t *frame, const JoystickPixels *p) {
  if (!frame || !p || !valid_center(p->left_x, p->left_y, 32) ||
      !valid_center(p->right_x, p->right_y, 95))
    return false;
  u8g2_t *u = U8G2_PortContext();
  if (!u)
    u = U8G2_PortInit(frame);
  else
    U8G2_PortBind(frame);
  u8g2_ClearBuffer(u);
  u8g2_SetDrawColor(u, 1);
  u8g2_DrawCircle(u, 32, 32, JOYSTICK_OUTER_RADIUS, U8G2_DRAW_ALL);
  u8g2_DrawCircle(u, 95, 32, JOYSTICK_OUTER_RADIUS, U8G2_DRAW_ALL);
  dot(u, p->left_x, p->left_y);
  dot(u, p->right_x, p->right_y);
  return true;
}
