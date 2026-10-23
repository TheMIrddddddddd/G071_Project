#ifndef APP_JOYSTICK_H
#define APP_JOYSTICK_H
#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Normalized inputs: +/-1, +X right and +Y up; singles are positive upward.
 * Foreground only. No peripheral access, logging, allocation or ISR work. */
typedef struct {
  float left_x, left_y, right_x, right_y, single_left, single_right;
} app_joystick_targets_t;

/* Starts centered, with independent simulated inputs for all four controls. */
bool app_joystick_init(uint64_t now_us);
/* Analytic spring sampling is bounded to 25 Hz. Rewinds are rejected. */
bool app_joystick_update(uint64_t now_us);
/* Switches from simulation to external targets until the next init.
 * Samples the old segment at now_us before retargeting to preserve velocity.
 * Finite inputs are clamped; invalid input/time is rejected unchanged. */
bool app_joystick_set_targets(uint64_t now_us, const app_joystick_targets_t *targets);
/* Draws the selected 06+07 layout in rows 24..58 without advancing the model. */
bool app_joystick_draw(u8g2_t *u);

#ifdef __cplusplus
}
#endif
#endif
