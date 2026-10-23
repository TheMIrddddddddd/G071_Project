#ifndef APP_BUS_H
#define APP_BUS_H
#include "board_soft_i2c.h"
#include <stdbool.h>
enum { APP_DEVICE_OLED, APP_DEVICE_RGB, APP_DEVICE_COUNT };
typedef struct { uint32_t errors, consecutive, last_ms, wait_ms; board_i2c_status_t last_error; } app_device_health_t;
typedef struct { app_device_health_t device[APP_DEVICE_COUNT]; uint32_t generation, recoveries, recovery_failures; bool blocked; } app_bus_status_t;
void app_bus_init(void);
void app_bus_poll(uint32_t now);
bool app_bus_can_try(unsigned device, uint32_t now);
void app_bus_success(unsigned device);
void app_bus_failure(unsigned device, board_i2c_status_t error, uint32_t now);
const app_bus_status_t *app_bus_status(void);
#endif
