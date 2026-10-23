#include "board_time.h"
#include "tim.h"
static uint32_t previous;
static uint64_t total;
void board_time_init(void) { previous = __HAL_TIM_GET_COUNTER(&htim2); total = 0U; }
uint64_t board_time_us(void)
{
  uint32_t now = __HAL_TIM_GET_COUNTER(&htim2);
  total += (uint32_t)(now - previous); previous = now; return total;
}
