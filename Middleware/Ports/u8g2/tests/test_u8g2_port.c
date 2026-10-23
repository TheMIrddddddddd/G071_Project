#include "u8g2_port.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

typedef struct
{
    uint8_t guard_before[16];
    uint8_t pixels[U8G2_PORT_BUFFER_SIZE];
    uint8_t guard_after[16];
} guarded_buffer_t;

static guarded_buffer_t first;
static guarded_buffer_t second;
static uint8_t expected[U8G2_PORT_BUFFER_SIZE];

static void check_guards(const guarded_buffer_t *buffer)
{
    size_t index;
    for (index = 0U; index < sizeof(buffer->guard_before); index++)
    {
        CHECK(buffer->guard_before[index] == 0xA5U);
        CHECK(buffer->guard_after[index] == 0xA5U);
    }
}

static uint8_t pixel(const uint8_t *buffer, unsigned int x, unsigned int y)
{
    return (uint8_t)((buffer[(y / 8U) * 128U + x] >> (y % 8U)) & 1U);
}

int main(void)
{
    u8g2_t *context;
    unsigned int x;
    unsigned int y;
    unsigned int count = 0U;
    size_t index;

    memset(&first, 0xA5, sizeof(first));
    memset(&second, 0xA5, sizeof(second));
    CHECK(U8G2_PortContext() == NULL);
    CHECK(U8G2_PortInit(NULL) == NULL);
    context = U8G2_PortInit(first.pixels);
    CHECK(context != NULL);
    CHECK(context == U8G2_PortContext());
    CHECK(u8g2_GetDisplayWidth(context) == 128U);
    CHECK(u8g2_GetDisplayHeight(context) == 64U);
    CHECK(u8g2_GetBufferSize(context) == 1024U);
    CHECK(u8g2_GetBufferPtr(context) == first.pixels);
    /* Transport messages are rejected, including a mistaken Init/Send attempt. */
    CHECK(context->u8x8.display_cb(&context->u8x8,
                                  U8X8_MSG_DISPLAY_INIT, 0U, NULL) == 0U);
    CHECK(context->u8x8.display_cb(&context->u8x8,
                                  U8X8_MSG_DISPLAY_DRAW_TILE, 0U, NULL) == 0U);

    /* Every pixel must map to exactly one expected bit, including all eight
     * page boundaries and all four corners. No hardware assumptions involved. */
    for (y = 0U; y < 64U; y++)
    {
        for (x = 0U; x < 128U; x++)
        {
            u8g2_ClearBuffer(context);
            memset(expected, 0, sizeof(expected));
            expected[(y / 8U) * 128U + x] = (uint8_t)(1U << (y % 8U));
            u8g2_DrawPixel(context, (u8g2_uint_t)x, (u8g2_uint_t)y);
            CHECK(memcmp(first.pixels, expected, sizeof(expected)) == 0);
        }
    }
    check_guards(&first);

    u8g2_ClearBuffer(context);
    u8g2_DrawFrame(context, 0U, 0U, 128U, 64U);
    for (y = 0U; y < 64U; y++)
    {
        for (x = 0U; x < 128U; x++)
        {
            CHECK(pixel(first.pixels, x, y) ==
                  (uint8_t)(x == 0U || x == 127U || y == 0U || y == 63U));
        }
    }
    u8g2_ClearBuffer(context);
    u8g2_DrawBox(context, 126U, 62U, 8U, 8U);
    u8g2_DrawPixel(context, 128U, 64U);
    for (y = 0U; y < 64U; y++)
    {
        for (x = 0U; x < 128U; x++)
        {
            CHECK(pixel(first.pixels, x, y) == (uint8_t)(x >= 126U && y >= 62U));
        }
    }
    check_guards(&first);

    /* A live transmission owns first: rebind and draw in second only. */
    memcpy(expected, first.pixels, sizeof(expected));
    U8G2_PortBind(second.pixels);
    CHECK(u8g2_GetBufferPtr(context) == second.pixels);
    U8G2_PortBind(NULL);
    CHECK(u8g2_GetBufferPtr(context) == second.pixels);
    u8g2_ClearBuffer(context);
    u8g2_DrawHLine(context, 3U, 7U, 20U);
    u8g2_DrawVLine(context, 8U, 3U, 20U);
    CHECK(memcmp(first.pixels, expected, sizeof(expected)) == 0);
    CHECK(pixel(second.pixels, 8U, 8U) != 0U);
    CHECK(pixel(second.pixels, 8U, 16U) != 0U);
    check_guards(&second);

    u8g2_ClearBuffer(context);
    /* Five-pixel advance, with the final I's right bearing excluded. */
    CHECK(u8g2_GetStrWidth(context, "DMA UI") == 29U);
    CHECK(u8g2_DrawStr(context, 2U, 9U, "DMA UI") == 30U);
    for (y = 0U; y < 64U; y++)
    {
        for (x = 0U; x < 128U; x++)
        {
            if (pixel(second.pixels, x, y) != 0U)
            {
                CHECK(x >= 2U && x < 32U && y >= 3U && y <= 9U);
                count++;
            }
        }
    }
    CHECK(count > 40U);
    u8g2_ClearBuffer(context);
    for (index = 0U; index < sizeof(second.pixels); index++)
    {
        CHECK(second.pixels[index] == 0U);
    }
    check_guards(&first);
    check_guards(&second);
    puts("PASS: 8192 pixel mappings, boundaries, clipping, ownership swap, real font rendering");
    return EXIT_SUCCESS;
}
