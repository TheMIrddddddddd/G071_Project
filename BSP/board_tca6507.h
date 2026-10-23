#ifndef BOARD_TCA6507_H
#define BOARD_TCA6507_H

#include "board_soft_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_TCA6507_ADDRESS 0x45U
/* Board wiring inherited from i2c_test: P0=green, P1=blue, P2=red. */
#define BOARD_RGB_OFF   0x00U
#define BOARD_RGB_GREEN 0x01U
#define BOARD_RGB_BLUE  0x02U
#define BOARD_RGB_RED   0x04U
#define BOARD_RGB_WHITE (BOARD_RGB_RED | BOARD_RGB_GREEN | BOARD_RGB_BLUE)

/* After software I2C init, foreground only, one serialized caller.
   Owns all seven outputs; unused P3-P6 stay off. No timer, logging or schedule.
   Init configures static output mode and turns all channels off. */
board_i2c_status_t board_tca6507_init(void);
/* Logical 1 lights the channel (output low), 0 releases it (off).
   Accepts any combination of the three RGB bits, rejects all other bits.
   Every call rewrites SELECT0/1/2, so a retry repairs a partial failed write.
   Error means actual output state is unconfirmed, not necessarily unchanged. */
board_i2c_status_t board_tca6507_set_rgb(uint8_t color);
typedef enum { BOARD_RGB_STATIC, BOARD_RGB_BLINK, BOARD_RGB_BREATHE } board_rgb_effect_t;
board_i2c_status_t board_tca6507_set_effect(board_rgb_effect_t effect, uint8_t color);

#ifdef __cplusplus
}
#endif
#endif
