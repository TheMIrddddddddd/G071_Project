#ifndef APP_RGB_H
#define APP_RGB_H
#include "board_tca6507.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { APP_RGB_CYCLE, APP_RGB_FIXED, APP_RGB_FLASH, APP_RGB_BREATH } app_rgb_mode_t;
typedef struct { app_rgb_mode_t mode; uint8_t color; bool valid; uint32_t writes, errors, max_late_ms; } app_rgb_status_t;
void app_rgb_init(uint32_t now);
bool app_rgb_set(app_rgb_mode_t mode, uint8_t color);
void app_rgb_poll(uint32_t now);
const app_rgb_status_t *app_rgb_status(void);
const char *app_rgb_color_name(void);
#ifdef __cplusplus
}
#endif
#endif
