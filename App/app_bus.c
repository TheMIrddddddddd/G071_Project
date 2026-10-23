#include "app_bus.h"
#include "board_usart.h"
#include <string.h>
static app_bus_status_t health;
static uint32_t recovery_tick, recovery_wait, attempts;
void app_bus_init(void) { memset(&health, 0, sizeof(health)); attempts = 0; }
const app_bus_status_t *app_bus_status(void) { return &health; }
bool app_bus_can_try(unsigned device, uint32_t now)
{
  if (device >= APP_DEVICE_COUNT || health.blocked) return false;
  const app_device_health_t *d = &health.device[device];
  return d->consecutive == 0U || (uint32_t)(now - d->last_ms) >= d->wait_ms;
}
void app_bus_success(unsigned device)
{
  if (device < APP_DEVICE_COUNT) health.device[device].consecutive = 0U;
}
void app_bus_failure(unsigned device, board_i2c_status_t error, uint32_t now)
{
  if (device >= APP_DEVICE_COUNT) return;
  app_device_health_t *d = &health.device[device];
  d->errors++; d->consecutive++; d->last_error = error; d->last_ms = now;
  d->wait_ms = (d->consecutive % 3U == 0U) ? 5000U : 500U;
  (void)board_usart3_printf("[FAULT] device=%u error=%s retry_ms=%lu\r\n", device,
    board_soft_i2c_status_string(error), (unsigned long)d->wait_ms);
  if ((error == BOARD_I2C_BUS_BUSY || error == BOARD_I2C_SCL_TIMEOUT) && !health.blocked)
  {
    health.blocked = true; health.generation++;
    attempts = 0U; recovery_tick = now; recovery_wait = 500U;
  }
}
void app_bus_poll(uint32_t now)
{
  if (!health.blocked || (uint32_t)(now - recovery_tick) < recovery_wait) return;
  recovery_tick = now; attempts++;
  board_i2c_status_t result = board_soft_i2c_recover();
  health.recoveries++;
  if (result == BOARD_I2C_OK)
  {
    health.blocked = false; health.generation++;
    for (unsigned i = 0; i < APP_DEVICE_COUNT; i++) health.device[i].consecutive = 0U;
  }
  else
  {
    health.recovery_failures++;
    recovery_wait = attempts == 3U ? 5000U : 500U;
    if (attempts == 3U) attempts = 0U;
  }
  (void)board_usart3_printf("[RECOVER] result=%s count=%lu next_ms=%lu\r\n",
    board_soft_i2c_status_string(result), (unsigned long)health.recoveries,
    (unsigned long)(health.blocked ? recovery_wait : 0U));
}
