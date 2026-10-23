#ifndef JOYSTICK_INPUT_DRAW_H
#define JOYSTICK_INPUT_DRAW_H
#include "joystick_follow.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    /* Circle centers are x/y; single_1/2 are vertical knob y coordinates. */
    int16_t left_x, left_y, right_x, right_y, single_1, single_2;
} JoystickInputPixels;
bool JoystickInput_Quantize(const JoystickSnapshot *snapshot, JoystickInputPixels *pixels);
bool JoystickInput_DrawFrame(uint8_t *frame, const JoystickInputPixels *pixels);
#ifdef __cplusplus
}
#endif
#endif
