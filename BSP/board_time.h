#ifndef BOARD_TIME_H
#define BOARD_TIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Foreground only. Read at least once per TIM2 wrap (~71 minutes).
   Never changes the timer configuration owned by software I2C. */
void board_time_init(void);
uint64_t board_time_us(void);
#ifdef __cplusplus
}
#endif
#endif
