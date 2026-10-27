#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H
#include "ui/components/animated_icon.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Six default, independently configurable instances. Modify their initial
 * configuration in status_bar.cpp. Instances and artwork are statically held. */
bool status_bar_init(uint64_t now_us);
/* Sample once when a frame is prepared; rendering itself does not advance time. */
bool status_bar_update(uint64_t now_us);
bool status_bar_draw(u8g2_t *u);
ui_icon_t *status_bar_get(ui_icon_type_t type);

#ifdef __cplusplus
}
#endif
#endif
