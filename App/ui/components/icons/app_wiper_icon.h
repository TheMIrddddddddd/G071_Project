#ifndef APP_WIPER_ICON_H
#define APP_WIPER_ICON_H
#include <stdint.h>
#include "u8g2.h"
/* 20x16 status slot from the approved wiper preview. Pure elapsed-time
   rendering: continuous sweeps while ON, parked while OFF; no I/O or heap. */
void app_wiper_icon_draw(u8g2_t *u,uint8_t x,uint8_t y,uint64_t elapsed_us);
int16_t app_wiper_icon_sample(uint64_t elapsed_us);
void app_wiper_icon_draw_state(u8g2_t *u,uint8_t x,uint8_t y,int16_t state);
#endif
