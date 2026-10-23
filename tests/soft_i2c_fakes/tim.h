#ifndef TEST_SOFT_I2C_TIM_H
#define TEST_SOFT_I2C_TIM_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef struct { uint32_t CR1, PSC, ARR; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { volatile uint32_t BSRR, IDR; } GPIO_TypeDef;
extern GPIO_TypeDef fake_gpio;
extern TIM_HandleTypeDef htim2;
extern uint32_t fake_ipsr, fake_primask;
#define TIM_CR1_CEN 1U
#define SOFT_I2C_SCL_Pin 32U
#define SOFT_I2C_SDA_Pin 64U
#define SOFT_I2C_SCL_GPIO_Port (&fake_gpio)
#define SOFT_I2C_SDA_GPIO_Port (&fake_gpio)
#define WRITE_REG(reg, value) fake_gpio_write(&(reg), (value))
#define READ_REG(reg) fake_gpio_read(&(reg))
#define __get_IPSR() (fake_ipsr)
#define __get_PRIMASK() (fake_primask)
#define __HAL_TIM_GET_COUNTER(handle) fake_counter()
uint32_t fake_counter(void);
void fake_gpio_write(volatile uint32_t *reg, uint32_t value);
uint32_t fake_gpio_read(const volatile uint32_t *reg);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetHCLKFreq(void);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *handle);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
#endif
