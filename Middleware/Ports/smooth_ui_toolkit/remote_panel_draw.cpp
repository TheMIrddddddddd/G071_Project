#include "remote_panel_draw.h"
#include "u8g2_port.h"
#include <algorithm>
#include <cmath>
namespace {
constexpr int cy=43, left_cx=24, right_cx=103, travel=13;
void circle_position(int x,int y,int cx,int16_t &out_x,int16_t &out_y)
{
    int dx=static_cast<int>(std::lround(static_cast<float>(y)*travel/JOYSTICK_MAX_OFFSET_PX));
    int dy=static_cast<int>(std::lround(static_cast<float>(-x)*travel/JOYSTICK_MAX_OFFSET_PX));
    while (dx*dx+dy*dy>travel*travel)
    {
        if (std::abs(dx)>=std::abs(dy)) dx += dx>0 ? -1 : 1;
        else dy += dy>0 ? -1 : 1;
    }
    out_x=static_cast<int16_t>(cx+dx); out_y=static_cast<int16_t>(cy+dy);
}
bool in_circle(int x,int y,int cx)
{
    int dx=x-cx,dy=y-cy;
    return std::abs(dx)<=travel && std::abs(dy)<=travel && dx*dx+dy*dy<=travel*travel;
}
void status_bar(u8g2_t *u,bool charging_visible)
{
    for (int bar=0;bar<4;++bar)
    {
        const int height=(bar+1)*3;
        u8g2_DrawBox(u,static_cast<u8g2_uint_t>(2+bar*4),static_cast<u8g2_uint_t>(17-height),2,
                     static_cast<u8g2_uint_t>(height));
    }
    u8g2_SetFont(u,u8g2_font_5x7_tf);
    u8g2_DrawStr(u,19,8,"RSSI"); u8g2_DrawStr(u,19,17,"98");
    // Fixed example battery level and charging animation; no hardware charging input yet.
    u8g2_DrawFrame(u,113,7,10,7); u8g2_DrawBox(u,123,9,2,3);
    for (int k=0;k<3;++k) u8g2_DrawBox(u,static_cast<u8g2_uint_t>(115+k*2),9,1,3);
    if (charging_visible)
    {
        const char *rows[]={"...##.","..###.","..##..",".#####","...##.","..##..","..#..."};
        for (int y=0;y<7;++y) for (int x=0;x<6;++x)
            if (rows[y][x]=='#') u8g2_DrawPixel(u,static_cast<u8g2_uint_t>(105+x),
                                            static_cast<u8g2_uint_t>(7+y));
    }
    u8g2_DrawHLine(u,0,19,128);
}
void dial(u8g2_t *u,int cx,int x,int y)
{
    u8g2_DrawCircle(u,static_cast<u8g2_uint_t>(cx),cy,17,U8G2_DRAW_ALL);
    u8g2_DrawHLine(u,static_cast<u8g2_uint_t>(cx-2),cy,5);
    u8g2_DrawVLine(u,static_cast<u8g2_uint_t>(cx),cy-2,5);
    u8g2_DrawDisc(u,static_cast<u8g2_uint_t>(x),static_cast<u8g2_uint_t>(y),3,U8G2_DRAW_ALL);
}
void slider(u8g2_t *u,int x,int y)
{
    u8g2_DrawFrame(u,static_cast<u8g2_uint_t>(x-1),28,3,31);
    u8g2_DrawBox(u,static_cast<u8g2_uint_t>(x-4),static_cast<u8g2_uint_t>(y-1),9,3);
}
}
bool RemotePanel_Quantize(const JoystickSnapshot *s,JoystickInputPixels *p)
{
    JoystickPixels base;
    if (!s || !p || !std::isfinite(s->single_current.x) || !std::isfinite(s->single_current.y) ||
        !Joystick_Quantize(s,&base)) return false;
    JoystickInputPixels result{};
    circle_position(base.left_x-32,base.left_y-32,left_cx,result.left_x,result.left_y);
    circle_position(base.right_x-95,base.right_y-32,right_cx,result.right_x,result.right_y);
    const auto single=[](float v) { return static_cast<int16_t>(cy-std::lround(
        std::clamp(v,-JOYSTICK_MAX_OFFSET_PX,JOYSTICK_MAX_OFFSET_PX)*13.0f/JOYSTICK_MAX_OFFSET_PX)); };
    result.single_1=single(s->single_current.x); result.single_2=single(s->single_current.y);
    *p=result; return true;
}
bool RemotePanel_ChargingVisible(uint64_t now_us)
{
    return (now_us/500000U)%2U==0U;
}
bool RemotePanel_DrawFrame(uint8_t *frame,const JoystickInputPixels *p,bool charging_visible)
{
    if (!frame || !p || !in_circle(p->left_x,p->left_y,left_cx) || !in_circle(p->right_x,p->right_y,right_cx) ||
        p->single_1<30 || p->single_1>56 || p->single_2<30 || p->single_2>56) return false;
    u8g2_t *u=U8G2_PortContext();
    if (!u) u=U8G2_PortInit(frame); else U8G2_PortBind(frame);
    u8g2_ClearBuffer(u); u8g2_SetDrawColor(u,1);
    status_bar(u,charging_visible);
    dial(u,left_cx,p->left_x,p->left_y); dial(u,right_cx,p->right_x,p->right_y);
    slider(u,56,p->single_1); slider(u,71,p->single_2);
    return true;
}
