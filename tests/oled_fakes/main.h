#ifndef TEST_OLED_MAIN_H
#define TEST_OLED_MAIN_H
#include <stdint.h>
extern uint32_t fake_ipsr, fake_primask;
#define __get_IPSR() (fake_ipsr)
#define __get_PRIMASK() (fake_primask)
uint32_t HAL_GetTick(void);
#endif
