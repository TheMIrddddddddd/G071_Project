#ifndef BOARD_USART_H
#define BOARD_USART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_USART3_TX_RINGBUFFER_SIZE     2048U
#define BOARD_USART3_TX_DMA_CHUNK    128U
#define BOARD_USART3_RX_DMA_BUFFER_SIZE     128U
#define BOARD_USART3_RX_RINGBUFFER_SIZE     512U
#define BOARD_USART3_LOG_FORMAT_SIZE     192U

typedef struct
{
  uint32_t tx_bytes;
  uint32_t tx_dma_starts;
  uint32_t tx_complete_events;
  uint32_t tx_queued_bytes;
  uint32_t tx_active;
  uint32_t tx_errors;
  uint32_t tx_timeouts;
  uint32_t tx_dropped_messages;
  uint32_t tx_dropped_bytes;
  uint32_t format_errors;
  uint32_t rx_bytes;
  uint32_t rx_read_bytes;
  uint32_t rx_dropped_bytes;
  uint32_t rx_idle_events;
  uint32_t rx_half_events;
  uint32_t rx_full_events;
  uint32_t rx_errors;
  uint32_t rx_restarts;
  uint32_t last_uart_error;
} board_usart3_stats_t;

/* For debugger observation; fields may change independently while running. */
extern volatile board_usart3_stats_t board_usart3_stats;

/* Foreground only, one producer/consumer, call Init once after UART/DMA init.
   Poll must run regularly. No heap allocation or waiting for queue space.
   Write/Printf enqueue an entire message or return 0 and count the drop.
   USART3 TX and RX are exclusively owned by this module. */
void board_usart3_init(void);
void board_usart3_poll(void);
uint16_t board_usart3_send_buffer(const uint8_t *data, uint16_t length);
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
uint16_t board_usart3_printf(const char *format, ...);
uint16_t board_usart3_receive(uint8_t *data, uint16_t capacity);

#ifdef __cplusplus
}
#endif
#endif
