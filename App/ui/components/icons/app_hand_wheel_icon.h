#ifndef APP_HAND_WHEEL_ICON_H
#define APP_HAND_WHEEL_ICON_H

#include <stdint.h>
#include "u8g2.h"

// Pure drawing: 20x16 hand/wheel preview driven by elapsed monotonic time.
void app_hand_wheel_icon_draw(u8g2_t *u, uint8_t x, uint8_t y, uint64_t elapsed_us);
int16_t app_hand_wheel_icon_sample(uint64_t elapsed_us);
void app_hand_wheel_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state);

#endif
