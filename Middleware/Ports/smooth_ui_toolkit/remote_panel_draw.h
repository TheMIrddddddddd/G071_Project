#ifndef REMOTE_PANEL_DRAW_H
#define REMOTE_PANEL_DRAW_H
#include "joystick_input_draw.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Six screen coordinates: centers (24/103,43), travel radius13, J3/J4 y=30..56. */
bool RemotePanel_Quantize(const JoystickSnapshot *snapshot, JoystickInputPixels *pixels);
/* Example charging indicator: 500ms on, 500ms off. Call with the monotonic UI clock. */
bool RemotePanel_ChargingVisible(uint64_t now_us);
/* RSSI98, battery level and charging indication are fixed demonstration data. */
bool RemotePanel_DrawFrame(uint8_t *frame, const JoystickInputPixels *pixels, bool charging_visible);
#ifdef __cplusplus
}
#endif
#endif
