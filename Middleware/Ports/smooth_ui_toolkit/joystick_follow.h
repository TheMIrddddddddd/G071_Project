#ifndef JOYSTICK_FOLLOW_H
#define JOYSTICK_FOLLOW_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define JOYSTICK_DEFAULT_STIFFNESS 100.0f
#define JOYSTICK_SPRING_MASS 1.0f
#define JOYSTICK_SNAP_PX 0.02f
#define JOYSTICK_OUTER_RADIUS 28
#define JOYSTICK_DOT_RADIUS_PX (16.0f / 3.0f)
/* One pixel geometric margin, followed by radius-safe integer quantization. */
#define JOYSTICK_MAX_OFFSET_PX (28.0f - JOYSTICK_DOT_RADIUS_PX - 1.0f)
#define JOYSTICK_PERIOD_US UINT64_C(10800000)
typedef struct {
  float x, y;
} JoystickPoint;
typedef struct {
  JoystickPoint left_target, right_target, left_current, right_current;
  uint64_t sample_us;
  uint8_t settled;
  /* Independent single-axis JOY3/JOY4; x and y are separate scalar channels. */
  JoystickPoint single_target, single_current;
} JoystickSnapshot;
typedef struct {
  int16_t left_x, left_y, right_x, right_y;
} JoystickPixels;

/* Same foreground caller only; no allocation or peripheral access.
 * Six fixed smooth_ui_toolkit::Spring objects, mass=1 and damping=2*sqrt(k*m).
 * Init explicitly resets state. Invalid stiffness/input/time is rejected unchanged.
 * Inputs are PIXEL OFFSETS from (32,32)/(95,32), +X right, +Y down.
 * Each stick is radially clamped to JOYSTICK_MAX_OFFSET_PX, independently. */
bool Joystick_Init(float stiffness);
/* Retunes all axes at their current position/velocity; keeps critical damping. */
bool Joystick_SetStiffness(float stiffness);
bool Joystick_SetTargets(float left_x, float left_y, float right_x,
                         float right_y);
bool Joystick_SetSingleTargets(float joy3, float joy4);
bool Joystick_Update(uint64_t now_us);
bool Joystick_GetSnapshot(JoystickSnapshot *snapshot);
bool Joystick_Quantize(const JoystickSnapshot *snapshot,
                       JoystickPixels *pixels);
/* Deterministic latest target only. Does not reset current state at period
 * wrap. */
bool Joystick_SetAutoTargets(uint64_t elapsed_us);
#ifdef __cplusplus
}
#endif
#endif
