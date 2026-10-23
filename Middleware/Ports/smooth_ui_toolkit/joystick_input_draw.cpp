#include "joystick_input_draw.h"
#include "u8g2_port.h"
#include <algorithm>
#include <cmath>
namespace {
int16_t circle_axis(int offset, int center) {
    return static_cast<int16_t>(center + std::lround(static_cast<float>(offset) * 9.0f / JOYSTICK_MAX_OFFSET_PX));
}
bool in_circle(int x, int y, int cy) {
    int dx = x-25, dy = y-cy;
    if (std::abs(dx) > 10 || std::abs(dy) > 10) return false;
    return dx*dx + dy*dy <= 100;
}
void slider(u8g2_t *u, int y, int center) {
    u8g2_DrawVLine(u,98,static_cast<u8g2_uint_t>(center-13),27);
    for (int tick : {center-13,center,center+13})
        u8g2_DrawHLine(u,95,static_cast<u8g2_uint_t>(tick),7);
    u8g2_DrawBox(u,94,static_cast<u8g2_uint_t>(y-2),9,5);
}
}
bool JoystickInput_Quantize(const JoystickSnapshot *s, JoystickInputPixels *p) {
    JoystickPixels circles;
    if (!s || !p || !Joystick_Quantize(s,&circles) ||
        !std::isfinite(s->single_current.x) || !std::isfinite(s->single_current.y)) return false;
    const auto single = [](float value, int center) {
        return static_cast<int16_t>(center - std::lround(
            std::clamp(value,-JOYSTICK_MAX_OFFSET_PX,JOYSTICK_MAX_OFFSET_PX) * 10.0f / JOYSTICK_MAX_OFFSET_PX));
    };
    /* Correct board mounting in this screen only: old right -> up, old down -> right. */
    *p = {circle_axis(circles.left_y-32,25),circle_axis(32-circles.left_x,17),
          circle_axis(circles.right_y-32,25),circle_axis(95-circles.right_x,48),
          single(s->single_current.x,17),single(s->single_current.y,48)};
    return true;
}
bool JoystickInput_DrawFrame(uint8_t *frame, const JoystickInputPixels *p) {
    if (!frame || !p || !in_circle(p->left_x,p->left_y,17) || !in_circle(p->right_x,p->right_y,48) ||
        p->single_1 < 7 || p->single_1 > 27 || p->single_2 < 38 || p->single_2 > 58) return false;
    u8g2_t *u = U8G2_PortContext();
    if (!u) u = U8G2_PortInit(frame); else U8G2_PortBind(frame);
    u8g2_ClearBuffer(u); u8g2_SetDrawColor(u,1);
    u8g2_DrawHLine(u,0,32,128); u8g2_DrawVLine(u,44,0,64);
    u8g2_SetFont(u,u8g2_font_5x7_tf);
    u8g2_DrawStr(u,1,8,"J1"); u8g2_DrawStr(u,1,39,"J2");
    u8g2_DrawStr(u,49,8,"J3"); u8g2_DrawStr(u,49,39,"J4");
    u8g2_DrawCircle(u,25,17,14,U8G2_DRAW_ALL); u8g2_DrawCircle(u,25,48,14,U8G2_DRAW_ALL);
    u8g2_DrawDisc(u,static_cast<u8g2_uint_t>(p->left_x),static_cast<u8g2_uint_t>(p->left_y),3,U8G2_DRAW_ALL);
    u8g2_DrawDisc(u,static_cast<u8g2_uint_t>(p->right_x),static_cast<u8g2_uint_t>(p->right_y),3,U8G2_DRAW_ALL);
    slider(u,p->single_1,17); slider(u,p->single_2,48);
    return true;
}
