#ifndef TEST_USART_H
#define TEST_USART_H
#include <stdint.h>

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { HAL_UART_RXEVENT_TC, HAL_UART_RXEVENT_HT,
               HAL_UART_RXEVENT_IDLE } HAL_UART_RxEventTypeTypeDef;
typedef struct { uint32_t remaining; uint32_t disabled; } DMA_HandleTypeDef;
typedef struct
{
  DMA_HandleTypeDef *hdmatx;
  DMA_HandleTypeDef *hdmarx;
  uint32_t ErrorCode;
} UART_HandleTypeDef;
extern UART_HandleTypeDef huart3;
extern uint32_t fake_ipsr;
extern uint32_t fake_primask;
#define DMA_IT_HT 1U
#define __get_IPSR() (fake_ipsr)
#define __get_PRIMASK() (fake_primask)
#define __disable_irq() ((void)(fake_primask = 1U))
#define __set_PRIMASK(value) ((void)(fake_primask = (value)))
#define __DMB() ((void)0)
#define __HAL_DMA_GET_COUNTER(dma) ((dma)->remaining)
#define __HAL_DMA_DISABLE_IT(dma, mask) ((void)((dma)->disabled |= (mask)))
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *, const uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(const UART_HandleTypeDef *);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *, uint16_t);
#endif
