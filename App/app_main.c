#include "app_main.h"
#include "app_bus.h"
#include "app_rgb.h"
#include "app_display.h"
#include "board_time.h"
#include "board_usart.h"
#include "main.h"
void app_main_init(void)
{
  board_usart3_init();app_bus_init();
  board_i2c_status_t result=board_soft_i2c_init();board_time_init();
  uint64_t now=board_time_us();uint32_t ms=(uint32_t)(now/1000U);
  (void)board_usart3_printf("\r\n[BOOT] G071 SYSCLK=%lu PB5=SCL PB6=SDA SoftI2C=%s\r\n",
    (unsigned long)HAL_RCC_GetSysClockFreq(),board_soft_i2c_status_string(result));
  if(result!=BOARD_I2C_OK) {
    app_bus_failure(APP_DEVICE_OLED,result,ms);app_bus_failure(APP_DEVICE_RGB,result,ms);
  }
  app_rgb_init(ms);app_display_init(now);
}
void app_main_poll(void)
{
  board_usart3_poll();uint64_t now=board_time_us();uint32_t ms=(uint32_t)(now/1000U);
  app_bus_poll(ms);app_rgb_poll(ms);app_display_poll(board_time_us());
}
