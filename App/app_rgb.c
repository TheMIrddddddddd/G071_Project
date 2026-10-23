#include "app_rgb.h"
#include "app_bus.h"
#include <string.h>
static app_rgb_status_t status;
static uint32_t last_tick, generation;
static uint8_t step_index, target;
static bool initialized, dirty, cycle_step_pending;
void app_rgb_init(uint32_t now)
{
  memset(&status, 0, sizeof(status)); status.mode = APP_RGB_CYCLE;
  last_tick = now; generation = app_bus_status()->generation;
  step_index = 0; target = 0; initialized = false; dirty = true; cycle_step_pending=false;
}
bool app_rgb_set(app_rgb_mode_t mode, uint8_t color)
{
  if (mode > APP_RGB_BREATH || (color & (uint8_t)~BOARD_RGB_WHITE)) return false;
  status.mode = mode; target = color; dirty = true;cycle_step_pending=false;
  if(mode==APP_RGB_CYCLE)step_index=0U;
  return true;
}
const app_rgb_status_t *app_rgb_status(void) { return &status; }
const char *app_rgb_color_name(void)
{
  if (!status.valid) return "UNKNOWN";
  static const char *names[] = {"OFF", "GREEN", "BLUE", "CYAN", "RED", "YELLOW", "MAGENTA", "WHITE"};
  return names[status.color & 7U];
}
void app_rgb_poll(uint32_t now)
{
  const app_bus_status_t *bus = app_bus_status();
  if (generation != bus->generation)
  { generation = bus->generation; initialized = false; dirty = true; status.valid = false; }
  if (!app_bus_can_try(APP_DEVICE_RGB, now)) return;
  board_i2c_status_t result;
  if (!initialized)
  {
    result = board_tca6507_init();
    if (result != BOARD_I2C_OK) goto failure;
    initialized = true;
  }
  if (status.mode == APP_RGB_CYCLE && !dirty)
  {
    uint32_t elapsed = (uint32_t)(now - last_tick);
    if (elapsed < 500U) return;
    uint32_t late = elapsed - 500U;
    if (late > status.max_late_ms) status.max_late_ms = late;
    static const uint8_t sequence[] = {4U, 1U, 2U, 7U, 0U};
    target = sequence[step_index]; dirty = true;cycle_step_pending=true;
  }
  if (!dirty) return;
  board_rgb_effect_t effect = status.mode == APP_RGB_FLASH ? BOARD_RGB_BLINK :
                             status.mode == APP_RGB_BREATH ? BOARD_RGB_BREATHE : BOARD_RGB_STATIC;
  result = board_tca6507_set_effect(effect, target);
  if (result != BOARD_I2C_OK) goto failure;
  app_bus_success(APP_DEVICE_RGB); status.writes++; status.valid = true; status.color = target;
  if (status.mode == APP_RGB_CYCLE && cycle_step_pending) step_index = (uint8_t)((step_index + 1U) % 5U);
  cycle_step_pending=false;
  dirty = false; last_tick = now; return;
failure:
  status.errors++; status.valid = false;
  app_bus_failure(APP_DEVICE_RGB, result, now);
}
