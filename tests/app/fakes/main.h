#ifndef HOST_APP_MAIN_H
#define HOST_APP_MAIN_H
#include <stdint.h>
extern uint32_t fake_ipsr, fake_primask;
#define __get_IPSR() fake_ipsr
#define __get_PRIMASK() fake_primask
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetSysClockFreq(void);
#endif
