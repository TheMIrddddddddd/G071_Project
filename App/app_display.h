#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H
#include <stdint.h>
void app_display_init(uint64_t now);
void app_display_poll(uint64_t now);
#endif
