#ifndef BOARD_SOFT_I2C_H
#define BOARD_SOFT_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  BOARD_I2C_OK = 0,
  BOARD_I2C_ADDRESS_NACK,
  BOARD_I2C_DATA_NACK,
  BOARD_I2C_SCL_TIMEOUT,
  BOARD_I2C_BUS_BUSY,
  BOARD_I2C_NOT_READY,
  BOARD_I2C_INVALID_ARGUMENT,
  BOARD_I2C_CONTEXT_ERROR
} board_i2c_status_t;

/* PB5=SCL, PB6=SDA; GPIO configuration belongs to CubeMX.
   Exclusive foreground owner, interrupts must remain enabled. No calls in ISR.
   TIM2 is exclusively owned here: 1 MHz, 32-bit free-running counter.
   Addresses are unshifted 7-bit values. Calls are synchronous and bounded by
   buffer lengths and a 2 ms timeout for each SCL rise. No logging in driver. */
board_i2c_status_t board_soft_i2c_init(void);
board_i2c_status_t board_soft_i2c_probe(uint8_t address);
/* Write, read, or combined write + repeated START + read. At least one length
   must be nonzero. Read ends with master NACK. Buffers live until return.
   A failed read may have filled only a prefix; disregard it on any error. */
board_i2c_status_t board_soft_i2c_transfer(uint8_t address,
                                         const uint8_t *tx, uint16_t tx_length,
                                         uint8_t *rx, uint16_t rx_length);
/* Explicit recovery: up to 9 clocks and STOP; no automatic device reset. */
board_i2c_status_t board_soft_i2c_recover(void);
const char *board_soft_i2c_status_string(board_i2c_status_t status);

#ifdef __cplusplus
}
#endif
#endif
