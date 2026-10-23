#ifndef OLED_FRAME_INFO_H
#define OLED_FRAME_INFO_H
#include <stdint.h>
/* Copied with the frame slot, never read from mutable live animation state. */
typedef struct
{
    uint32_t anim_us;
    int32_t target_x_mpx, target_y_mpx, current_x_mpx, current_y_mpx;
    uint8_t valid;
    /* Existing X/Y remain the first/left pair; dual=0 preserves v24/v25 logs. */
    uint8_t dual;
    int32_t right_target_x_mpx, right_target_y_mpx, right_current_x_mpx, right_current_y_mpx;
} OLED_AnimationInfo;
#endif
