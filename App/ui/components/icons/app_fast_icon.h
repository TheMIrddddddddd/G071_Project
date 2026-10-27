#ifndef APP_FAST_ICON_H
#define APP_FAST_ICON_H
#include <stdint.h>
#include "u8g2.h"
/* 20x16 quick-mode indicator: crossed-out outline OFF, filled lightning ON.
   Pure rendering from elapsed time, no I/O or dynamic allocation. */
void app_fast_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us);
int16_t app_fast_icon_sample(uint64_t elapsed_us);
void app_fast_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state);
#endif
