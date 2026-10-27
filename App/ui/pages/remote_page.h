#ifndef REMOTE_PAGE_H
#define REMOTE_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "u8g2.h"

/* Single foreground context; drawing never advances component animation. */
bool remote_page_init(uint64_t now_us);
bool remote_page_update(uint64_t now_us);
bool remote_page_draw(u8g2_t *display);

#endif
