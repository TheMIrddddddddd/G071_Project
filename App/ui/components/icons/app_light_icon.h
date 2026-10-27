#ifndef APP_LIGHT_ICON_H
#define APP_LIGHT_ICON_H
#include <stdint.h>
#include "u8g2.h"
/* Draws an original 20x16 monochrome icon into the caller's cleared buffer.
   Pure rendering: elapsed time is supplied by the application, no I/O/heap. */
void app_light_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us);
int16_t app_light_icon_sample(uint64_t elapsed_us);
void app_light_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state);
#endif
