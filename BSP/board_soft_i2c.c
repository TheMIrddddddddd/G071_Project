#include "board_soft_i2c.h"
#include "tim.h"

/* Each phase waits 3 TIM2 counts (1 us/count), plus GPIO/software/IRQ overhead.
   Measure actual bus timing; this setting alone does not guarantee Standard-mode.
   Never mask IRQs for a transaction. */
#define I2C_PHASE_US       3U
#define I2C_SCL_TIMEOUT_US 2000U
static uint8_t initialized;

static void scl_set(GPIO_PinState state)
{
  const uint32_t pin = SOFT_I2C_SCL_Pin;
  /* BSRR lower half releases open-drain output; upper half drives it low.
     WRITE_REG expands to one volatile store, without touching other pins. */
  WRITE_REG(SOFT_I2C_SCL_GPIO_Port->BSRR,
            (state == GPIO_PIN_SET) ? pin : (pin << 16U));
}

static void sda_set(GPIO_PinState state)
{
  const uint32_t pin = SOFT_I2C_SDA_Pin;
  WRITE_REG(SOFT_I2C_SDA_GPIO_Port->BSRR,
            (state == GPIO_PIN_SET) ? pin : (pin << 16U));
}

static GPIO_PinState sda_get(void)
{
  return ((READ_REG(SOFT_I2C_SDA_GPIO_Port->IDR) & SOFT_I2C_SDA_Pin) != 0U)
           ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void delay_phase(void)
{
  const uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
  while ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < I2C_PHASE_US)
  {
    /* Free-running timer, wrap-safe subtraction. IRQs remain enabled. */
  }
}

static board_i2c_status_t check_ready(void)
{
  if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
  {
    return BOARD_I2C_CONTEXT_ERROR;
  }
  if ((initialized == 0U) || ((htim2.Instance->CR1 & TIM_CR1_CEN) == 0U))
  {
    return BOARD_I2C_NOT_READY;
  }
  return BOARD_I2C_OK;
}

static void release_bus(void)
{
  sda_set(GPIO_PIN_SET);
  scl_set(GPIO_PIN_SET);
}

static board_i2c_status_t scl_high(void)
{
  scl_set(GPIO_PIN_SET);
  const uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
  while ((READ_REG(SOFT_I2C_SCL_GPIO_Port->IDR) & SOFT_I2C_SCL_Pin) == 0U)
  {
    if ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) >= I2C_SCL_TIMEOUT_US)
    {
      return BOARD_I2C_SCL_TIMEOUT;
    }
  }
  delay_phase();
  return BOARD_I2C_OK;
}

static board_i2c_status_t start_condition(void)
{
  sda_set(GPIO_PIN_SET);
  delay_phase();
  board_i2c_status_t status = scl_high();
  if (status != BOARD_I2C_OK) return status;
  if (sda_get() == GPIO_PIN_RESET) return BOARD_I2C_BUS_BUSY;
  sda_set(GPIO_PIN_RESET);
  delay_phase();
  scl_set(GPIO_PIN_RESET);
  /* First data bit supplies the low-phase wait after setting SDA. */
  return BOARD_I2C_OK;
}

static board_i2c_status_t stop_condition(void)
{
  scl_set(GPIO_PIN_RESET);
  sda_set(GPIO_PIN_RESET);
  delay_phase();
  board_i2c_status_t status = scl_high();
  if (status != BOARD_I2C_OK)
  {
    release_bus();
    return status;
  }
  sda_set(GPIO_PIN_SET);
  delay_phase();
  return (sda_get() == GPIO_PIN_SET) ? BOARD_I2C_OK : BOARD_I2C_BUS_BUSY;
}

static board_i2c_status_t write_byte(uint8_t value)
{
  for (uint32_t bit = 0; bit < 8U; bit++)
  {
    sda_set((value & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_phase();
    board_i2c_status_t status = scl_high();
    if (status != BOARD_I2C_OK) return status;
    scl_set(GPIO_PIN_RESET);
    /* Next data/ACK setup supplies the single low-phase wait. */
    value = (uint8_t)(value << 1);
  }
  sda_set(GPIO_PIN_SET);
  delay_phase();
  board_i2c_status_t status = scl_high();
  if (status != BOARD_I2C_OK) return status;
  const GPIO_PinState ack = sda_get();
  scl_set(GPIO_PIN_RESET);
  return (ack == GPIO_PIN_RESET) ? BOARD_I2C_OK : BOARD_I2C_DATA_NACK;
}

static board_i2c_status_t send_address(uint8_t address, uint8_t read)
{
  const board_i2c_status_t status = write_byte((uint8_t)((address << 1) | read));
  return (status == BOARD_I2C_DATA_NACK) ? BOARD_I2C_ADDRESS_NACK : status;
}

static board_i2c_status_t read_byte(uint8_t *value, uint8_t last)
{
  uint8_t data = 0;
  sda_set(GPIO_PIN_SET);
  for (uint32_t bit = 0; bit < 8U; bit++)
  {
    delay_phase();
    board_i2c_status_t status = scl_high();
    if (status != BOARD_I2C_OK) return status;
    data = (uint8_t)(((uint32_t)data << 1) | ((sda_get() == GPIO_PIN_SET) ? 1U : 0U));
    scl_set(GPIO_PIN_RESET);
  }
  sda_set(last ? GPIO_PIN_SET : GPIO_PIN_RESET);
  delay_phase();
  board_i2c_status_t status = scl_high();
  if (status != BOARD_I2C_OK) return status;
  scl_set(GPIO_PIN_RESET);
  sda_set(GPIO_PIN_SET);
  *value = data;
  return BOARD_I2C_OK;
}

static board_i2c_status_t finish(board_i2c_status_t status)
{
  if ((status == BOARD_I2C_SCL_TIMEOUT) || (status == BOARD_I2C_BUS_BUSY))
  {
    /* No valid STOP is guaranteed on a stuck bus. Release both outputs. */
    release_bus();
    return status;
  }
  const board_i2c_status_t stop_status = stop_condition();
  return (stop_status == BOARD_I2C_OK) ? status : stop_status;
}

board_i2c_status_t board_soft_i2c_init(void)
{
  if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U)) return BOARD_I2C_CONTEXT_ERROR;
  initialized = 0;
  /* This board uses APB /1. Check actual timer configuration rather than
     silently accepting a changed CubeMX clock or a 16-bit counter period. */
  if ((HAL_RCC_GetPCLK1Freq() != 16000000U) ||
      (HAL_RCC_GetHCLKFreq() != HAL_RCC_GetPCLK1Freq()) ||
      (htim2.Instance->PSC != 15U) ||
      (htim2.Instance->ARR != UINT32_MAX)) return BOARD_I2C_NOT_READY;
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) return BOARD_I2C_NOT_READY;
  initialized = 1;
  release_bus();
  const board_i2c_status_t status = scl_high();
  if (status != BOARD_I2C_OK) return status;
  return (sda_get() == GPIO_PIN_SET) ? BOARD_I2C_OK : BOARD_I2C_BUS_BUSY;
}

board_i2c_status_t board_soft_i2c_probe(uint8_t address)
{
  board_i2c_status_t status = check_ready();
  if (status != BOARD_I2C_OK) return status;
  if (address > 0x7FU) return BOARD_I2C_INVALID_ARGUMENT;
  status = start_condition();
  if (status == BOARD_I2C_OK) status = send_address(address, 0U);
  return finish(status);
}

board_i2c_status_t board_soft_i2c_transfer(uint8_t address,
                                         const uint8_t *tx, uint16_t tx_length,
                                         uint8_t *rx, uint16_t rx_length)
{
  board_i2c_status_t status = check_ready();
  if (status != BOARD_I2C_OK) return status;
  if ((address > 0x7FU) || ((tx_length == 0U) && (rx_length == 0U)) ||
      ((tx_length != 0U) && (tx == 0)) || ((rx_length != 0U) && (rx == 0)))
    return BOARD_I2C_INVALID_ARGUMENT;
  status = start_condition();
  if (status != BOARD_I2C_OK) return finish(status);
  status = send_address(address, (tx_length == 0U) ? 1U : 0U);
  for (uint16_t i = 0; (i < tx_length) && (status == BOARD_I2C_OK); i++)
    status = write_byte(tx[i]);
  if ((tx_length != 0U) && (rx_length != 0U) && (status == BOARD_I2C_OK))
  {
    status = start_condition();
    if (status == BOARD_I2C_OK) status = send_address(address, 1U);
  }
  for (uint16_t i = 0; (i < rx_length) && (status == BOARD_I2C_OK); i++)
    status = read_byte(&rx[i], (i == (uint16_t)(rx_length - 1U)) ? 1U : 0U);
  return finish(status);
}

board_i2c_status_t board_soft_i2c_recover(void)
{
  board_i2c_status_t status = check_ready();
  if (status != BOARD_I2C_OK) return status;
  sda_set(GPIO_PIN_SET);
  status = scl_high();
  for (uint32_t i = 0; (i < 9U) && (status == BOARD_I2C_OK) &&
                        (sda_get() == GPIO_PIN_RESET); i++)
  {
    scl_set(GPIO_PIN_RESET);
    delay_phase();
    status = scl_high();
  }
  return finish(status);
}

const char *board_soft_i2c_status_string(board_i2c_status_t status)
{
  switch (status)
  {
    case BOARD_I2C_OK: return "OK";
    case BOARD_I2C_ADDRESS_NACK: return "ADDR_NACK";
    case BOARD_I2C_DATA_NACK: return "DATA_NACK";
    case BOARD_I2C_SCL_TIMEOUT: return "SCL_TIMEOUT";
    case BOARD_I2C_BUS_BUSY: return "BUS_BUSY";
    case BOARD_I2C_NOT_READY: return "NOT_READY";
    case BOARD_I2C_INVALID_ARGUMENT: return "INVALID_ARG";
    case BOARD_I2C_CONTEXT_ERROR: return "CONTEXT_ERROR";
    default: return "UNKNOWN";
  }
}
