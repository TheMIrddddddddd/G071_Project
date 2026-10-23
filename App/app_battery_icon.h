#ifndef APP_BATTERY_ICON_H
#define APP_BATTERY_ICON_H

#include <stdint.h>
#include "u8g2.h"

// Pure drawing: 20x16 charging/1..4-bar simulation driven by elapsed time.
void app_battery_icon_draw(u8g2_t *u, uint8_t x, uint8_t y, uint64_t elapsed_us);

#endif
