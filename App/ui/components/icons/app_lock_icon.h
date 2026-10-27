#ifndef APP_LOCK_ICON_H
#define APP_LOCK_ICON_H
#include <stdint.h>
#include "u8g2.h"
/* 20x16 status slot. Pure rendering from elapsed time; no I/O or heap. */
void app_lock_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us);
int16_t app_lock_icon_sample(uint64_t elapsed_us);
void app_lock_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state);
#endif
