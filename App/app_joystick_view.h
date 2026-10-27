#ifndef APP_JOYSTICK_VIEW_H
#define APP_JOYSTICK_VIEW_H

#include "joystick_follow.h"
#include "u8g2.h"

/* Render an already sampled snapshot; does not advance the animation. */
void app_joystick_view_draw(u8g2_t *u, const JoystickSnapshot &state);

#endif
