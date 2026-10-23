#ifndef U8G2_PORT_H
#define U8G2_PORT_H

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define U8G2_PORT_WIDTH 128U
#define U8G2_PORT_HEIGHT 64U
#define U8G2_PORT_BUFFER_SIZE 1024U

/* Main-loop-only singleton. The caller owns a persistent, writable 1024-byte
 * buffer. Init configures drawing only and never sends controller commands.
 * NULL is rejected. Init returns NULL in that case without changing context. */
u8g2_t *U8G2_PortInit(uint8_t *draw_buffer);

/* Bind only a buffer owned by the application for drawing, never an active
 * transmit buffer. Does not clear/redraw it; the application redraws a complete
 * candidate after each ownership swap. NULL leaves the current binding intact. */
void U8G2_PortBind(uint8_t *draw_buffer);
u8g2_t *U8G2_PortContext(void);

#ifdef __cplusplus
}
#endif

#endif
