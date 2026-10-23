#include "board_usart.h"
#include "usart.h"
#include "ringbuffer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TX_TIMEOUT_MS  250U
#define RETRY_MS       10U
#define EVENT_TX_DONE  (1UL << 0)
#define EVENT_ERROR    (1UL << 1)

_Static_assert(BOARD_USART3_TX_RINGBUFFER_SIZE <= 32764U,
               "Ringbuffer uses 15-bit indices");
_Static_assert(BOARD_USART3_RX_RINGBUFFER_SIZE <= 32764U,
               "Ringbuffer uses 15-bit indices");
_Static_assert((BOARD_USART3_TX_RINGBUFFER_SIZE % 4U) == 0U,
               "Ringbuffer pool size must be a multiple of four");
_Static_assert((BOARD_USART3_RX_RINGBUFFER_SIZE % 4U) == 0U,
               "Ringbuffer pool size must be a multiple of four");

static struct
{
  struct rt_ringbuffer tx_queue;
  struct rt_ringbuffer rx_queue;
  uint8_t tx_pool[BOARD_USART3_TX_RINGBUFFER_SIZE];
  uint8_t rx_pool[BOARD_USART3_RX_RINGBUFFER_SIZE];
  uint8_t rx_dma[BOARD_USART3_RX_DMA_BUFFER_SIZE];
  char format[BOARD_USART3_LOG_FORMAT_SIZE];
  volatile uint32_t events; /* Equal-priority UART/DMA ISRs produce. */
  uint32_t tx_tick;
  uint32_t tx_attempt_tick;
  uint32_t rx_attempt_tick;
  uint16_t tx_length;
  uint16_t rx_position;
  uint8_t tx_abort;
  uint8_t rx_restart;
} log3;

volatile board_usart3_stats_t board_usart3_stats;

static uint32_t take_events(void)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const uint32_t events = log3.events;
  log3.events = 0U;
  __set_PRIMASK(primask);
  return events;
}

static void rx_copy(const uint8_t *data, uint16_t length)
{
  /* ISR is the only RX producer. Foreground RX reads mask interrupts. */
  const rt_size_t accepted = rt_ringbuffer_put(&log3.rx_queue, data, length);
  board_usart3_stats.rx_bytes += length;
  board_usart3_stats.rx_dropped_bytes += (uint32_t)((rt_size_t)length - accepted);
}

void board_usart3_init(void)
{
  memset(&log3, 0, sizeof(log3));
  memset((void *)&board_usart3_stats, 0, sizeof(board_usart3_stats));
  rt_ringbuffer_init(&log3.tx_queue, log3.tx_pool,
                     BOARD_USART3_TX_RINGBUFFER_SIZE);
  rt_ringbuffer_init(&log3.rx_queue, log3.rx_pool,
                     BOARD_USART3_RX_RINGBUFFER_SIZE);
  const uint32_t now = HAL_GetTick();
  log3.tx_attempt_tick = now - RETRY_MS;
  log3.rx_attempt_tick = now - RETRY_MS;
  log3.rx_restart = 1U;
  board_usart3_poll();
}

uint16_t board_usart3_send_buffer(const uint8_t *data, uint16_t length)
{
  if ((__get_IPSR() != 0U) || (data == NULL) || (length == 0U))
  {
    return 0U;
  }
  const uint32_t queued = (uint32_t)rt_ringbuffer_data_len(&log3.tx_queue);
  if (length > (BOARD_USART3_TX_RINGBUFFER_SIZE - queued))
  {
    board_usart3_stats.tx_dropped_messages++;
    board_usart3_stats.tx_dropped_bytes += length;
    return 0U;
  }
  /* Only foreground accesses the TX ring. DMA holds the peeked span until
     completion; the consumer index advances only in board_usart3_poll(). */
  (void)rt_ringbuffer_put(&log3.tx_queue, data, length);
  board_usart3_stats.tx_queued_bytes = queued + length;
  return length;
}

uint16_t board_usart3_printf(const char *format, ...)
{
  if ((__get_IPSR() != 0U) || (format == NULL))
  {
    return 0U;
  }
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(log3.format, sizeof(log3.format), format, args);
  va_end(args);
  if ((length < 0) || ((size_t)length >= sizeof(log3.format)))
  {
    board_usart3_stats.format_errors++;
    board_usart3_stats.tx_dropped_messages++;
    if (length > 0)
    {
      board_usart3_stats.tx_dropped_bytes += (uint32_t)length;
    }
    return 0U;
  }
  return board_usart3_send_buffer((const uint8_t *)log3.format, (uint16_t)length);
}

uint16_t board_usart3_receive(uint8_t *data, uint16_t capacity)
{
  if ((__get_IPSR() != 0U) || (data == NULL) || (capacity == 0U))
  {
    return 0U;
  }
  /* Imported middleware has no internal synchronization. Keep its RX read
     atomic against ISR puts; copy is bounded by the 512-byte RX pool. */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const rt_size_t length = rt_ringbuffer_get(&log3.rx_queue, data, capacity);
  __set_PRIMASK(primask);
  board_usart3_stats.rx_read_bytes += (uint32_t)length;
  return (uint16_t)length;
}

void board_usart3_poll(void)
{
  const uint32_t now = HAL_GetTick();
  const uint32_t events = take_events();
  if ((events & EVENT_ERROR) != 0U)
  {
    /* HAL DMA errors may end both directions. Recover only in foreground. */
    board_usart3_stats.rx_errors++;
    log3.rx_restart = 1U;
    if (log3.tx_length != 0U)
    {
      log3.tx_abort = 1U;
      board_usart3_stats.tx_errors++;
    }
  }
  else if (((events & EVENT_TX_DONE) != 0U) && (log3.tx_abort == 0U))
  {
    (void)rt_ringbuffer_discard(&log3.tx_queue, log3.tx_length);
    board_usart3_stats.tx_bytes += log3.tx_length;
    log3.tx_length = 0U;
    board_usart3_stats.tx_active = 0U;
    board_usart3_stats.tx_queued_bytes = (uint32_t)rt_ringbuffer_data_len(&log3.tx_queue);
  }

  if ((log3.tx_length != 0U) && (log3.tx_abort == 0U)
      && ((uint32_t)(now - log3.tx_tick) >= TX_TIMEOUT_MS))
  {
    log3.tx_abort = 1U;
    board_usart3_stats.tx_timeouts++;
    board_usart3_stats.tx_errors++;
  }
  if ((log3.tx_abort != 0U)
      && ((uint32_t)(now - log3.tx_attempt_tick) >= RETRY_MS))
  {
    log3.tx_attempt_tick = now;
    if (HAL_UART_AbortTransmit(&huart3) == HAL_OK)
    {
      log3.tx_abort = 0U;
      log3.tx_length = 0U;
      board_usart3_stats.tx_active = 0U;
      /* Ring read position is unchanged; retry after the abort. */
    }
  }

  if ((log3.rx_restart != 0U)
      && ((uint32_t)(now - log3.rx_attempt_tick) >= RETRY_MS))
  {
    log3.rx_attempt_tick = now;
    if (HAL_UART_AbortReceive(&huart3) == HAL_OK)
    {
      log3.rx_position = 0U;
      if (HAL_UARTEx_ReceiveToIdle_DMA(&huart3, log3.rx_dma,
                                      BOARD_USART3_RX_DMA_BUFFER_SIZE) == HAL_OK)
      {
        log3.rx_restart = 0U;
        board_usart3_stats.rx_restarts++;
      }
      else
      {
        board_usart3_stats.rx_errors++;
      }
    }
  }

  if ((log3.tx_length == 0U) && (log3.tx_abort == 0U)
      && (rt_ringbuffer_data_len(&log3.tx_queue) != 0U)
      && ((uint32_t)(now - log3.tx_attempt_tick) >= RETRY_MS))
  {
    const uint8_t *data;
    uint32_t length = (uint32_t)rt_ringbuffer_peek_linear(&log3.tx_queue, &data);
    if (length > BOARD_USART3_TX_DMA_CHUNK)
    {
      length = BOARD_USART3_TX_DMA_CHUNK;
    }
    log3.tx_length = (uint16_t)length;
    log3.tx_tick = now;
    log3.tx_attempt_tick = now;
    __DMB();
    if (HAL_UART_Transmit_DMA(&huart3, data,
                              log3.tx_length) == HAL_OK)
    {
      __HAL_DMA_DISABLE_IT(huart3.hdmatx, DMA_IT_HT);
      board_usart3_stats.tx_dma_starts++;
      board_usart3_stats.tx_active = 1U;
    }
    else
    {
      log3.tx_length = 0U;
      board_usart3_stats.tx_errors++;
    }
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart3)
  {
    board_usart3_stats.tx_complete_events++;
    log3.events |= EVENT_TX_DONE;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart3)
  {
    board_usart3_stats.last_uart_error = huart->ErrorCode;
    log3.events |= EVENT_ERROR;
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  (void)size;
  if (huart != &huart3)
  {
    return;
  }
  const HAL_UART_RxEventTypeTypeDef event = HAL_UARTEx_GetRxEventType(huart);
  if (event == HAL_UART_RXEVENT_IDLE)
  {
    board_usart3_stats.rx_idle_events++;
  }
  else if (event == HAL_UART_RXEVENT_HT)
  {
    board_usart3_stats.rx_half_events++;
  }
  else
  {
    board_usart3_stats.rx_full_events++;
  }

  /* Read the live DMA cursor instead of possibly stale HT/TC callback sizes.
     Equal IRQ priorities prevent nested RX producers. Keep HT and TC enabled:
     service RX at least once per half buffer (about 5.56 ms at 115200 8N1).
     A complete unserviced DMA lap cannot be reconstructed from CNDTR. */
  const uint32_t remaining = __HAL_DMA_GET_COUNTER(huart->hdmarx);
  if (remaining > BOARD_USART3_RX_DMA_BUFFER_SIZE)
  {
    log3.events |= EVENT_ERROR;
    return;
  }
  uint16_t position = (uint16_t)(BOARD_USART3_RX_DMA_BUFFER_SIZE - remaining);
  if (position == BOARD_USART3_RX_DMA_BUFFER_SIZE)
  {
    position = 0U;
  }
  const uint16_t previous = log3.rx_position;
  if (position < previous)
  {
    rx_copy(&log3.rx_dma[previous], (uint16_t)(BOARD_USART3_RX_DMA_BUFFER_SIZE - previous));
    if (position != 0U)
    {
      rx_copy(log3.rx_dma, position);
    }
  }
  else if (position > previous)
  {
    rx_copy(&log3.rx_dma[previous], (uint16_t)(position - previous));
  }
  log3.rx_position = position;
}
