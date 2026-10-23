#include "board_usart.h"
#include "usart.h"
#include "ringbuffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static DMA_HandleTypeDef tx_dma, rx_dma;
UART_HandleTypeDef huart3 = { &tx_dma, &rx_dma, 0U };
uint32_t fake_ipsr, fake_primask;
static uint32_t tick;
static const uint8_t *active_tx;
static uint16_t active_length;
static uint8_t *receive_buffer;
static uint16_t receive_size, receive_position;
static HAL_UART_RxEventTypeTypeDef rx_event;
static HAL_StatusTypeDef tx_start_result, tx_abort_result, rx_start_result, rx_abort_result;
static uint8_t transmitted[16384];
static size_t transmitted_size;
static unsigned tx_aborts, rx_starts;

uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart,
                                       const uint8_t *data, uint16_t length)
{
  assert(uart == &huart3);
  assert(active_tx == NULL);
  assert(length > 0U && length <= BOARD_USART3_TX_DMA_CHUNK);
  if (tx_start_result == HAL_OK)
  {
    active_tx = data;
    active_length = length;
  }
  return tx_start_result;
}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart,
                                              uint8_t *data, uint16_t length)
{
  assert(uart == &huart3 && length == BOARD_USART3_RX_DMA_BUFFER_SIZE);
  if (rx_start_result == HAL_OK)
  {
    receive_buffer = data;
    receive_size = length;
    receive_position = 0U;
    rx_dma.remaining = length;
    rx_starts++;
  }
  return rx_start_result;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart)
{
  assert(uart == &huart3);
  tx_aborts++;
  if (tx_abort_result == HAL_OK)
  {
    active_tx = NULL;
    active_length = 0U;
  }
  return tx_abort_result;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart)
{
  assert(uart == &huart3);
  return rx_abort_result;
}
HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(const UART_HandleTypeDef *uart)
{
  assert(uart == &huart3);
  return rx_event;
}

static void reset_at(uint32_t start)
{
  active_tx = NULL;
  active_length = 0U;
  fake_ipsr = fake_primask = 0U;
  tick = start;
  tx_start_result = tx_abort_result = rx_start_result = rx_abort_result = HAL_OK;
  transmitted_size = 0U;
  tx_aborts = rx_starts = 0U;
  huart3.ErrorCode = 0U;
  board_usart3_init();
  assert(rx_starts == 1U);
}
static void complete_tx(void)
{
  assert(active_tx != NULL);
  assert(transmitted_size + active_length <= sizeof(transmitted));
  memcpy(&transmitted[transmitted_size], active_tx, active_length);
  transmitted_size += active_length;
  active_tx = NULL;
  active_length = 0U;
  fake_ipsr = 1U;
  HAL_UART_TxCpltCallback(&huart3);
  fake_ipsr = 0U;
}
static void drain_tx(void)
{
  unsigned guard = 0U;
  while (board_usart3_stats.tx_queued_bytes != 0U)
  {
    assert(++guard < 1000U);
    tick += 12U;
    board_usart3_poll();
    if (active_tx != NULL)
    {
      complete_tx();
      board_usart3_poll();
    }
  }
}
static void receive_bytes(const uint8_t *data, uint16_t length)
{
  for (uint16_t i = 0U; i < length; i++)
  {
    receive_buffer[receive_position++] = data[i];
    rx_dma.remaining = (uint32_t)(receive_size - receive_position);
    if (receive_position == receive_size / 2U)
    {
      rx_event = HAL_UART_RXEVENT_HT;
      fake_ipsr = 1U;
      HAL_UARTEx_RxEventCallback(&huart3, receive_position);
      fake_ipsr = 0U;
    }
    if (receive_position == receive_size)
    {
      receive_position = 0U;
      rx_dma.remaining = receive_size;
      rx_event = HAL_UART_RXEVENT_TC;
      fake_ipsr = 1U;
      HAL_UARTEx_RxEventCallback(&huart3, receive_size);
      fake_ipsr = 0U;
    }
  }
  rx_event = HAL_UART_RXEVENT_IDLE;
  fake_ipsr = 1U;
  HAL_UARTEx_RxEventCallback(&huart3, receive_position);
  fake_ipsr = 0U;
}
static void pattern(uint8_t *data, size_t length)
{
  for (size_t i = 0U; i < length; i++) { data[i] = (uint8_t)(i * 37U); }
}

static void test_tx_order_wrap_and_lifetime(void)
{
  reset_at(0U);
  uint8_t a[1500], b[1300], saved[128];
  pattern(a, sizeof(a)); pattern(b, sizeof(b));
  assert(board_usart3_send_buffer(a, sizeof(a)) == sizeof(a));
  board_usart3_poll();
  assert(active_tx != NULL && active_length == 128U);
  memcpy(saved, active_tx, sizeof(saved));
  assert(board_usart3_send_buffer(b, 200U) == 200U);
  assert(memcmp(saved, active_tx, sizeof(saved)) == 0);
  drain_tx();
  assert(board_usart3_send_buffer(b, sizeof(b)) == sizeof(b));
  drain_tx();
  assert(transmitted_size == sizeof(a) + 200U + sizeof(b));
  assert(memcmp(transmitted, a, sizeof(a)) == 0);
  assert(memcmp(transmitted + sizeof(a), b, 200U) == 0);
  assert(memcmp(transmitted + sizeof(a) + 200U, b, sizeof(b)) == 0);
  assert(board_usart3_stats.tx_bytes == transmitted_size);
}
static void test_tx_full_is_atomic_and_inflight_is_reserved(void)
{
  reset_at(0U);
  uint8_t data[2048]; pattern(data, sizeof(data));
  assert(board_usart3_send_buffer(data, sizeof(data)) == sizeof(data));
  assert(board_usart3_send_buffer(data, 1U) == 0U);
  board_usart3_poll();
  assert(board_usart3_send_buffer(data, 1U) == 0U);
  assert(board_usart3_stats.tx_dropped_messages == 2U);
  complete_tx(); tick += 12U; board_usart3_poll();
  assert(board_usart3_send_buffer(data, 1U) == 1U);
  drain_tx();
  assert(transmitted_size == 2049U);
  assert(memcmp(transmitted, data, sizeof(data)) == 0);
}
static void test_hal_busy_retry(void)
{
  reset_at(0U);
  assert(board_usart3_printf("hello %u", 42U) == 8U);
  tx_start_result = HAL_BUSY;
  board_usart3_poll();
  assert(board_usart3_stats.tx_errors == 1U && active_tx == NULL);
  assert(board_usart3_stats.tx_queued_bytes == 8U);
  tx_start_result = HAL_OK;
  drain_tx();
  assert(transmitted_size == 8U && memcmp(transmitted, "hello 42", 8U) == 0);
}
static void test_timeout_abort_failure_preserves_buffer(void)
{
  reset_at(0U);
  uint8_t data[2048]; pattern(data, sizeof(data));
  assert(board_usart3_send_buffer(data, sizeof(data)) == sizeof(data));
  board_usart3_poll();
  const uint8_t *original = active_tx;
  tx_abort_result = HAL_TIMEOUT;
  tick = 250U; board_usart3_poll();
  assert(board_usart3_stats.tx_timeouts == 1U && tx_aborts == 1U);
  assert(active_tx == original && memcmp(active_tx, data, 128U) == 0);
  assert(board_usart3_send_buffer(data, 1U) == 0U);
  tx_abort_result = HAL_OK;
  tick += 10U; board_usart3_poll();
  assert(active_tx == NULL && board_usart3_stats.tx_queued_bytes == 2048U);
  drain_tx();
  assert(transmitted_size == sizeof(data));
  assert(memcmp(transmitted, data, sizeof(data)) == 0);
}
static void test_format_limits_and_isr_rejection(void)
{
  reset_at(0U);
  char text[193]; memset(text, 'a', sizeof(text)); text[192] = '\0';
  assert(board_usart3_printf("%s", text) == 0U);
  assert(board_usart3_stats.format_errors == 1U);
  text[191] = '\0';
  assert(board_usart3_printf("%s", text) == 191U);
  fake_ipsr = 1U;
  assert(board_usart3_printf("ISR") == 0U);
  assert(board_usart3_send_buffer((const uint8_t *)"ISR", 3U) == 0U);
  fake_ipsr = 0U;
  drain_tx(); assert(transmitted_size == 191U);
}
static void test_rx_idle_half_full_and_wrap(void)
{
  reset_at(0U);
  uint8_t data[300], out[300]; pattern(data, sizeof(data));
  receive_bytes(data, 97U);
  assert(board_usart3_receive(out, 75U) == 75U);
  receive_bytes(data + 97U, 203U);
  assert(board_usart3_receive(out + 75U, 225U) == 225U);
  assert(memcmp(out, data, sizeof(data)) == 0);
  assert(board_usart3_stats.rx_bytes == sizeof(data));
  assert(board_usart3_stats.rx_half_events == 2U);
  assert(board_usart3_stats.rx_full_events == 2U);
  assert(board_usart3_stats.rx_idle_events == 2U);
  assert(board_usart3_receive(out, sizeof(out)) == 0U);
}
static void test_rx_duplicate_and_stale_events(void)
{
  reset_at(0U);
  uint8_t data[100], out[100]; pattern(data, sizeof(data));
  receive_bytes(data, 100U);
  rx_event = HAL_UART_RXEVENT_HT;
  /* A delayed HT size must not move the already sampled cursor backwards. */
  HAL_UARTEx_RxEventCallback(&huart3, 64U);
  rx_event = HAL_UART_RXEVENT_IDLE;
  HAL_UARTEx_RxEventCallback(&huart3, 100U);
  assert(board_usart3_receive(out, sizeof(out)) == sizeof(out));
  assert(memcmp(out, data, sizeof(out)) == 0);
  assert(board_usart3_stats.rx_bytes == sizeof(data));
}
static void test_rx_overflow_and_ring_wrap(void)
{
  reset_at(0U);
  uint8_t data[700], out[700]; pattern(data, sizeof(data));
  receive_bytes(data, sizeof(data));
  assert(board_usart3_stats.rx_dropped_bytes == 188U);
  assert(board_usart3_receive(out, 400U) == 400U);
  receive_bytes(data, 300U);
  assert(board_usart3_receive(out + 400U, 112U) == 112U);
  assert(memcmp(out, data, 512U) == 0);
  assert(board_usart3_receive(out, 300U) == 300U);
  assert(memcmp(out, data, 300U) == 0);
}
static void test_error_recovery_and_restart_retry(void)
{
  reset_at(0U);
  assert(board_usart3_printf("pending") == 7U);
  board_usart3_poll();
  huart3.ErrorCode = 8U;
  HAL_UART_ErrorCallback(&huart3);
  rx_start_result = HAL_ERROR;
  tick += 10U; board_usart3_poll();
  assert(tx_aborts == 1U && board_usart3_stats.tx_queued_bytes == 7U);
  assert(board_usart3_stats.last_uart_error == 8U && rx_starts == 1U);
  rx_start_result = HAL_OK;
  tick += 10U; board_usart3_poll();
  assert(rx_starts == 2U);
  drain_tx(); assert(memcmp(transmitted, "pending", 7U) == 0);
}
static void test_tick_wrap_and_irq_mask_restore(void)
{
  const uint32_t start = UINT32_MAX - 5U;
  reset_at(start);
  assert(board_usart3_printf("wrap") == 4U);
  board_usart3_poll();
  fake_primask = 1U;
  tick = start + 249U; board_usart3_poll();
  assert(fake_primask == 1U && board_usart3_stats.tx_timeouts == 0U);
  tick = start + 250U; board_usart3_poll();
  assert(board_usart3_stats.tx_timeouts == 1U);
  fake_primask = 0U;
  drain_tx();
}

static void test_middleware_dma_peek_and_discard(void)
{
  struct rt_ringbuffer ring;
  uint8_t pool[8];
  const uint8_t *span;
  rt_ringbuffer_init(&ring, pool, 8);
  assert(rt_ringbuffer_put(&ring, (const uint8_t *)"ABCDEFGH", 8U) == 8U);
  assert(rt_ringbuffer_peek_linear(&ring, &span) == 8U);
  assert(memcmp(span, "ABCDEFGH", 8U) == 0);
  assert(rt_ringbuffer_data_len(&ring) == 8U);
  assert(rt_ringbuffer_discard(&ring, 3U) == 3U);
  assert(rt_ringbuffer_put(&ring, (const uint8_t *)"IJK", 3U) == 3U);
  assert(rt_ringbuffer_peek_linear(&ring, &span) == 5U);
  assert(memcmp(span, "DEFGH", 5U) == 0);
  assert(rt_ringbuffer_data_len(&ring) == 8U);
  assert(rt_ringbuffer_discard(&ring, 5U) == 5U);
  assert(rt_ringbuffer_peek_linear(&ring, &span) == 3U);
  assert(memcmp(span, "IJK", 3U) == 0);
  assert(rt_ringbuffer_discard(&ring, 99U) == 3U);
  assert(rt_ringbuffer_peek_linear(&ring, &span) == 0U && span == NULL);
}

int main(void)
{
  test_middleware_dma_peek_and_discard();
  test_tx_order_wrap_and_lifetime();
  test_tx_full_is_atomic_and_inflight_is_reserved();
  test_hal_busy_retry();
  test_timeout_abort_failure_preserves_buffer();
  test_format_limits_and_isr_rejection();
  test_rx_idle_half_full_and_wrap();
  test_rx_duplicate_and_stale_events();
  test_rx_overflow_and_ring_wrap();
  test_error_recovery_and_restart_retry();
  test_tick_wrap_and_irq_mask_restore();
  puts("PASS: 11 BSP UART3 DMA + middleware ringbuffer tests");
  return 0;
}
