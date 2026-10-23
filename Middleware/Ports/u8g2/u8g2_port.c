#include "u8g2_port.h"

#include <stddef.h>
#include <string.h>

static u8g2_t drawing_context;
static uint8_t initialized;

/* Use upstream's genuine SH1106 geometry, but only its memory setup message.
 * Reject all display/transport messages: hardware setup, contrast, power and
 * pixel transfers belong exclusively to the application's BSP display driver. */
static uint8_t drawing_display_cb(u8x8_t *display, uint8_t message,
                                 uint8_t argument, void *data)
{
    if (message == U8X8_MSG_DISPLAY_SETUP_MEMORY)
    {
        return u8x8_d_sh1106_128x64_noname(display, message, argument, data);
    }
    return 0U;
}

u8g2_t *U8G2_PortInit(uint8_t *draw_buffer)
{
    if (draw_buffer == NULL)
    {
        return NULL;
    }

    memset(&drawing_context, 0, sizeof(drawing_context));
    u8g2_SetupDisplay(&drawing_context, drawing_display_cb,
                     u8x8_dummy_cb, u8x8_dummy_cb, u8x8_dummy_cb);
    /* Avoid generated constructors and their hidden static framebuffer. */
    u8g2_SetupBuffer(&drawing_context, draw_buffer, 8U,
                    u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
    u8g2_SetFont(&drawing_context, u8g2_font_5x7_tf);
    u8g2_SetFontMode(&drawing_context, 1U);
    initialized = 1U;
    return &drawing_context;
}

void U8G2_PortBind(uint8_t *draw_buffer)
{
    if ((initialized != 0U) && (draw_buffer != NULL))
    {
        u8g2_SetBufferPtr(&drawing_context, draw_buffer);
    }
}

u8g2_t *U8G2_PortContext(void)
{
    return initialized != 0U ? &drawing_context : NULL;
}
