#ifndef APP_UI_H
#define APP_UI_H
#include <stdint.h>
#include <stdbool.h>
/* Enabled by the LAYOUT build mode; existing host SHOWCASE tests default off. */
#ifndef APP_LAYOUT
#define APP_LAYOUT 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t uptime, frames, last_ms, errors; const char *rgb; } app_ui_status_t;
bool app_ui_init(uint64_t now, uint8_t *bootstrap);
bool app_ui_update(uint64_t now);
bool app_ui_draw(uint8_t *frame, const app_ui_status_t *status);
uint32_t app_ui_heap_peak(void);
bool app_ui_failed(void);
#ifdef __cplusplus
}
#endif
#endif
