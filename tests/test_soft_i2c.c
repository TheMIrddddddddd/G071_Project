#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tim.h"
#include "board_soft_i2c.h"
#include "board_time.h"

static TIM_TypeDef timer;
TIM_HandleTypeDef htim2 = { &timer };
GPIO_TypeDef fake_gpio;
uint32_t fake_ipsr, fake_primask;
static uint32_t now;
static int scl, sda, stuck_scl, stuck_sda, active, phase;
static int starts, stops, edges, nack_byte, byte_count, read_mode;
static int release_after, stretch_reads;
static int segment_bytes, read_count, master_ack[8], fail_edge;
static uint8_t byte_value, bytes[32];
static uint32_t last_rise, last_fall, max_data_period, min_data_low, min_data_high;

uint32_t fake_counter(void) { return now++; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 16000000U; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return 16000000U; }
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *handle)
{ handle->Instance->CR1 |= TIM_CR1_CEN; return HAL_OK; }
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
  (void)port;
  const int high = state == GPIO_PIN_SET;
  if (pin == SOFT_I2C_SDA_Pin)
  {
    if (scl && !stuck_scl && sda != high)
    {
      if (!high) { active = 1; phase = 0; byte_value = 0; read_mode = 0; segment_bytes = 0; starts++; }
      else { active = 0; stops++; }
    }
    sda = high;
  }
  else
  {
    if (!scl && high)
    {
      edges++;
      if (fail_edge > 0 && edges == fail_edge) stuck_scl = 1;
      if (release_after > 0 && edges >= release_after) stuck_sda = 0;
      if (active)
      {
        phase++;
        if (byte_count == 0 && phase >= 2 && phase <= 8)
        {
          const uint32_t period = now - last_rise;
          const uint32_t low = now - last_fall;
          if (period > max_data_period) max_data_period = period;
          if (low < min_data_low) min_data_low = low;
        }
        if (phase <= 8) byte_value = (uint8_t)(((uint32_t)byte_value << 1) | (sda ? 1U : 0U));
        if (phase == 9)
        {
          if (byte_count < 32) bytes[byte_count] = byte_value;
          byte_count++;
          segment_bytes++;
          if (read_mode && read_count < 8) master_ack[read_count++] = sda;
        }
      }
    }
    if (!scl && high) last_rise = now;
    if (scl && !high)
    {
      if (active && byte_count == 0 && phase >= 1 && phase <= 8)
      {
        const uint32_t high_time = now - last_rise;
        if (high_time < min_data_high) min_data_high = high_time;
      }
      last_fall = now;
    }
    if (scl && !high && phase == 9)
    {
      if (segment_bytes == 1 && byte_count > 0 && (bytes[byte_count - 1] & 1U)) read_mode = 1;
      phase = 0; byte_value = 0;
    }
    scl = high;
  }
}
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
  (void)port;
  if (pin == SOFT_I2C_SCL_Pin)
  {
    if (stretch_reads > 0) { stretch_reads--; return GPIO_PIN_RESET; }
    return (scl && !stuck_scl) ? GPIO_PIN_SET : GPIO_PIN_RESET;
  }
  if (stuck_sda || !sda) return GPIO_PIN_RESET;
  if (active && read_mode && phase >= 1 && phase <= 8)
  {
    const uint8_t value = (read_count == 0) ? 0xA5U : 0x3CU;
    return (((uint32_t)value >> (8 - phase)) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
  }
  if (active && phase == 9 && !read_mode && byte_count != nack_byte) return GPIO_PIN_RESET;
  return GPIO_PIN_SET;
}
/* CMSIS WRITE_REG/READ_REG retain production BSRR/IDR access semantics.
   Decode atomic set/reset halves; no ODR read-modify-write is modeled. */
void fake_gpio_write(volatile uint32_t *reg, uint32_t value)
{
  assert(reg == &fake_gpio.BSRR);
  const uint32_t allowed = SOFT_I2C_SCL_Pin | SOFT_I2C_SDA_Pin;
  assert((value & ~(allowed | (allowed << 16))) == 0U);
  assert(value != 0U && (value & (value - 1U)) == 0U);
  const uint16_t pin = (uint16_t)((value & allowed) | (value >> 16));
  HAL_GPIO_WritePin(&fake_gpio, pin, (value & allowed) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
uint32_t fake_gpio_read(const volatile uint32_t *reg)
{
  assert(reg == &fake_gpio.IDR);
  return ((HAL_GPIO_ReadPin(&fake_gpio, SOFT_I2C_SCL_Pin) == GPIO_PIN_SET) ? SOFT_I2C_SCL_Pin : 0U) |
         ((HAL_GPIO_ReadPin(&fake_gpio, SOFT_I2C_SDA_Pin) == GPIO_PIN_SET) ? SOFT_I2C_SDA_Pin : 0U);
}
static void reset_bus(void)
{
  timer.CR1 = 0; timer.PSC = 15; timer.ARR = UINT32_MAX;
  fake_ipsr = 0; fake_primask = 0; now = 0;
  scl = sda = 1; stuck_scl = stuck_sda = active = phase = 0;
  starts = stops = edges = byte_count = read_mode = release_after = stretch_reads = 0;
  nack_byte = -1; byte_value = 0; memset(bytes, 0, sizeof(bytes));
  segment_bytes = read_count = fail_edge = 0; memset(master_ack, 0, sizeof(master_ack));
  last_rise = last_fall = max_data_period = 0;
  min_data_low = min_data_high = UINT32_MAX;
  assert(board_soft_i2c_init() == BOARD_I2C_OK);
}
int main(void)
{
  reset_bus();
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_OK);
  assert(starts == 1 && stops == 1 && bytes[0] == 0x78 && scl && sda);
  /* Current profile uses 3 us phases. Model has zero GPIO call overhead. Each data bit needs one low wait and
     one high wait; allow counter-read overhead, reject duplicate low waits. */
  assert(max_data_period >= 6U && max_data_period <= 12U);
  assert(min_data_low >= 3U && min_data_high >= 3U);
  reset_bus(); nack_byte = 1;
  assert(board_soft_i2c_probe(0x45) == BOARD_I2C_ADDRESS_NACK);
  assert(bytes[0] == 0x8A && stops == 1);
  nack_byte = -1;
  assert(board_soft_i2c_probe(0x45) == BOARD_I2C_OK);
  reset_bus(); stuck_scl = 1;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_SCL_TIMEOUT);
  assert(now < 10000 && scl && sda);
  assert(board_soft_i2c_recover() == BOARD_I2C_SCL_TIMEOUT);
  reset_bus(); stuck_sda = 1;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_BUS_BUSY);
  assert(board_soft_i2c_recover() == BOARD_I2C_BUS_BUSY);
  assert(edges <= 10);
  reset_bus(); stuck_sda = 1; release_after = 5;
  assert(board_soft_i2c_recover() == BOARD_I2C_OK);
  assert(scl && sda);
  reset_bus(); stretch_reads = 20; now = UINT32_MAX - 10U;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_OK);
  reset_bus(); fake_ipsr = 1;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_CONTEXT_ERROR);
  fake_ipsr = 0; fake_primask = 1;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_CONTEXT_ERROR);
  fake_primask = 0;
  assert(board_soft_i2c_probe(0x80) == BOARD_I2C_INVALID_ARGUMENT);
  timer.CR1 = 0;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_NOT_READY);
  reset_bus();
  const uint8_t data[] = {0x12, 0xA5};
  assert(board_soft_i2c_transfer(0x45, data, 2, NULL, 0) == BOARD_I2C_OK);
  assert(bytes[0] == 0x8A && bytes[1] == 0x12 && bytes[2] == 0xA5);
  reset_bus(); nack_byte = 2;
  assert(board_soft_i2c_transfer(0x45, data, 2, NULL, 0) == BOARD_I2C_DATA_NACK);
  assert(stops == 1 && byte_count == 2);
  reset_bus(); uint8_t result = 0;
  assert(board_soft_i2c_transfer(0x45, data, 1, &result, 1) == BOARD_I2C_OK);
  assert(starts == 2 && stops == 1 && bytes[2] == 0x8B && result == 0xA5);
  assert(master_ack[0] == 1);
  reset_bus(); uint8_t received[2] = {0};
  assert(board_soft_i2c_transfer(0x45, NULL, 0, received, 2) == BOARD_I2C_OK);
  assert(bytes[0] == 0x8B && received[0] == 0xA5 && received[1] == 0x3C);
  assert(master_ack[0] == 0 && master_ack[1] == 1 && stops == 1);
  reset_bus(); fail_edge = 4;
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_SCL_TIMEOUT);
  assert(scl && sda && now < 10000);
  reset_bus(); timer.PSC = 16;
  assert(board_soft_i2c_init() == BOARD_I2C_NOT_READY);
  assert(board_soft_i2c_probe(0x3C) == BOARD_I2C_NOT_READY);
  reset_bus();
  assert(board_soft_i2c_transfer(0x45, NULL, 1, NULL, 0) == BOARD_I2C_INVALID_ARGUMENT);
  assert(board_soft_i2c_transfer(0x45, NULL, 0, NULL, 0) == BOARD_I2C_INVALID_ARGUMENT);
  reset_bus();board_time_init();now=UINT32_MAX-5U;
  const uint64_t first_time=board_time_us();now=7U;
  assert(board_time_us()-first_time==13U);
  puts("PASS: software I2C address/ACK/NACK/timeout/recovery/wrap/context/write/read and 64-bit TIM2 time tests");
  return 0;
}
