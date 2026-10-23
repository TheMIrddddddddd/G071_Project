#ifndef JOYSTICK_DRAW_H
#define JOYSTICK_DRAW_H
#include "joystick_follow.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Caller owns the full 1024-byte writable frame. Pure drawing: no I2C/DMA.
 * Reject invalid centers without touching the frame; redraw all four circles.
 */
bool Joystick_DrawFrame(uint8_t *frame, const JoystickPixels *pixels);
#ifdef __cplusplus
}
#endif
#endif
