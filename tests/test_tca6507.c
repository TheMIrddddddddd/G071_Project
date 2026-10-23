#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board_tca6507.h"

static unsigned int calls;
static uint8_t packet[12];
static board_i2c_status_t response = BOARD_I2C_OK;

board_i2c_status_t board_soft_i2c_transfer(uint8_t address,
                                         const uint8_t *tx, uint16_t tx_length,
                                         uint8_t *rx, uint16_t rx_length)
{
  assert(address == 0x45U && tx != NULL && (tx_length == 4U || tx_length == 12U));
  assert(rx == NULL && rx_length == 0U);
  memcpy(packet, tx, tx_length);
  calls++;
  return response;
}

int main(void)
{
  assert(board_tca6507_init() == BOARD_I2C_OK);
  const uint8_t off[] = {0x10, 0, 0, 0};
  assert(memcmp(packet, off, sizeof(off)) == 0);
  assert(BOARD_RGB_RED == 4U && BOARD_RGB_GREEN == 1U && BOARD_RGB_BLUE == 2U);
  for (uint8_t color = 0; color < 8U; color++)
  {
    assert(board_tca6507_set_rgb(color) == BOARD_I2C_OK);
    assert(packet[0] == 0x10U && packet[1] == 0U && packet[2] == 0U);
    assert(packet[3] == color);
  }
  const unsigned int before = calls;
  assert(board_tca6507_set_rgb(8) == BOARD_I2C_INVALID_ARGUMENT);
  assert(board_tca6507_set_rgb(0x80) == BOARD_I2C_INVALID_ARGUMENT);
  assert(calls == before);
  const board_i2c_status_t errors[] = {
    BOARD_I2C_ADDRESS_NACK, BOARD_I2C_DATA_NACK, BOARD_I2C_SCL_TIMEOUT,
    BOARD_I2C_BUS_BUSY, BOARD_I2C_NOT_READY, BOARD_I2C_CONTEXT_ERROR
  };
  for (unsigned int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
  {
    response = errors[i];
    assert(board_tca6507_set_rgb(BOARD_RGB_RED) == errors[i]);
    assert(board_tca6507_init() == errors[i]);
  }
  response = BOARD_I2C_OK;
  assert(board_tca6507_set_rgb(BOARD_RGB_RED) == BOARD_I2C_OK);
  assert(packet[0] == 0x10U && packet[1] == 0 && packet[2] == 0 && packet[3] == 4U);
  assert(board_tca6507_set_effect(BOARD_RGB_BLINK,4)==BOARD_I2C_OK);
  const uint8_t blink[]={0x10,0,4,4,0,0x44,0,0x44,0x44,0xFF,0,8};
  assert(memcmp(packet,blink,12)==0);
  assert(board_tca6507_set_effect(BOARD_RGB_BREATHE,7)==BOARD_I2C_OK);
  const uint8_t breathe[]={0x10,0,7,7,0x66,0x11,0x66,0x22,0x22,0xFF,0,8};
  assert(memcmp(packet,breathe,12)==0);
  assert(board_tca6507_set_effect((board_rgb_effect_t)99,1)==BOARD_I2C_INVALID_ARGUMENT);
  puts("PASS: TCA6507 static RGB packets, mapping, masks, errors and retry");
  return 0;
}
