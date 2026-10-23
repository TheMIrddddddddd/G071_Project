#include "board_tca6507.h"

/* TI TCA6507 Rev.D, command bit 4 enables register auto-increment.
   SELECT2:SELECT1:SELECT0 = 100 drives output low (fully on),
   000 releases output (off). PWM banks are not used for this basic test. */
#define TCA6507_REG_SELECT0 0x00U
#define TCA6507_AUTO_INCREMENT 0x10U

board_i2c_status_t board_tca6507_init(void)
{
  return board_tca6507_set_rgb(BOARD_RGB_OFF);
}

board_i2c_status_t board_tca6507_set_rgb(uint8_t color)
{
  if ((color & (uint8_t)~BOARD_RGB_WHITE) != 0U)
  {
    return BOARD_I2C_INVALID_ARGUMENT;
  }
  /* Configure all three selectors, independent of prior PWM/output state.
     RGB changes normally affect only SELECT2; unused outputs stay off.
     This synchronous transfer finishes before the stack buffer expires. */
  const uint8_t registers[] = {
    TCA6507_AUTO_INCREMENT | TCA6507_REG_SELECT0,
    0x00U, /* SELECT0 */
    0x00U, /* SELECT1 */
    color  /* SELECT2 */
  };
  return board_soft_i2c_transfer(BOARD_TCA6507_ADDRESS,
                                 registers, (uint16_t)sizeof(registers), 0, 0U);
}

board_i2c_status_t board_tca6507_set_effect(board_rgb_effect_t effect, uint8_t color)
{
  if (effect == BOARD_RGB_STATIC) return board_tca6507_set_rgb(color);
  if ((color & (uint8_t)~BOARD_RGB_WHITE) || (effect != BOARD_RGB_BLINK && effect != BOARD_RGB_BREATHE))
    return BOARD_I2C_INVALID_ARGUMENT;
  /* BANK0 (110). Timing nibbles duplicated for both banks, but only BANK0 is
     selected. Blink: 256ms on/off. Breathe: 512ms fade, 64ms on, 128ms off.
     Register 0A bit3 starts BANK0 from fade-on; only written on effect change. */
  uint8_t packet[] = {0x10U, 0U, color, color, 0U, 0x44U, 0U, 0x44U, 0x44U, 0xFFU, 0U, 0x08U};
  if (effect == BOARD_RGB_BREATHE)
  { packet[4] = 0x66U; packet[5] = 0x11U; packet[6] = 0x66U; packet[7] = packet[8] = 0x22U; }
  return board_soft_i2c_transfer(BOARD_TCA6507_ADDRESS, packet, (uint16_t)sizeof(packet), 0, 0U);
}
