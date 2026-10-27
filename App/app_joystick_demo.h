#ifndef APP_JOYSTICK_DEMO_H
#define APP_JOYSTICK_DEMO_H

#include "app_joystick.h"

/* Return the latest independent demo targets for elapsed microseconds. */
app_joystick_targets_t app_joystick_demo_targets(uint64_t elapsed_us);

#endif
