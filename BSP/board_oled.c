#include "board_oled.h"
#include "main.h"
#include <string.h>

#define OLED_COLUMN_OFFSET 2U
#define OLED_RAM_COLUMNS 132U
#define OLED_CHUNK_SIZE 32U
/* From oled_test/Core/Src/oled_display.c: keep panel orientation and contrast.
   20/02 is retained from that proven legacy sequence, not interpreted as an
   SH1106 horizontal-addressing command. Actual writes always set page/column. */
static const uint8_t init_packet[] = {
  0x00, 0xAE, 0x02, 0x10, 0x40, 0x81, 0xFF, 0xA1, 0xC8, 0xA6,
  0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12,
  0xDB, 0x40, 0x20, 0x02, 0xA4, 0xA6
};
static board_oled_status_t status;
static const uint8_t *image;
static uint32_t state_tick;
static uint32_t frame_tick;
static uint8_t panel_on;
static const OLED_RefreshPlan *active_plan;
static uint16_t span_index, span_offset;

static board_i2c_status_t send(const uint8_t *bytes, uint16_t length)
{
  board_i2c_status_t result = board_soft_i2c_transfer(BOARD_OLED_ADDRESS, bytes, length, 0, 0U);
  if (result == BOARD_I2C_OK) status.wire_bytes += (uint32_t)length + 1U;
  return result;
}

static board_i2c_status_t position(uint8_t offset)
{
  const uint8_t column = (uint8_t)(status.column + offset);
  const uint8_t packet[] = {0x00U, (uint8_t)(0xB0U | status.page),
                            (uint8_t)(column & 15U), (uint8_t)(0x10U | (column >> 4))};
  return send(packet, (uint16_t)sizeof(packet));
}

static board_i2c_status_t data_chunk(uint8_t clear)
{
  uint8_t packet[OLED_CHUNK_SIZE + 1U];
  const uint8_t width = clear ? OLED_RAM_COLUMNS : BOARD_OLED_WIDTH;
  uint8_t length = (uint8_t)(width - status.column);
  if (!clear && active_plan)
    length = (uint8_t)(active_plan->spans[span_index].length - span_offset);
  if (length > OLED_CHUNK_SIZE) length = OLED_CHUNK_SIZE;
  packet[0] = 0x40U;
  if (clear) memset(packet + 1, 0, length);
  else memcpy(packet + 1, image + (uint16_t)status.page * BOARD_OLED_WIDTH + status.column, length);
  const board_i2c_status_t result = send(packet, (uint16_t)(length + 1U));
  if (result != BOARD_I2C_OK) return result;
  if (!clear && active_plan)
  {
    span_offset = (uint16_t)(span_offset + length);
    if (span_offset == active_plan->spans[span_index].length)
    { span_index++; span_offset = 0U; }
    if (span_index == active_plan->span_count)
      status.state = BOARD_OLED_READY;
    else
    {
      status.page = active_plan->spans[span_index].page;
      status.column = (uint8_t)(active_plan->spans[span_index].column + span_offset);
      status.state = BOARD_OLED_FRAME_POSITION;
    }
    return BOARD_I2C_OK;
  }
  status.column = (uint8_t)(status.column + length);
  if (status.column == width)
  {
    status.column = 0U;
    status.page++;
  }
  if (status.page == 8U)
  {
    status.page = 0U;
    status.state = clear ? BOARD_OLED_FRAME_POSITION :
                   (panel_on ? BOARD_OLED_READY : BOARD_OLED_DISPLAY_ON);
  }
  else status.state = clear ? BOARD_OLED_CLEAR_POSITION : BOARD_OLED_FRAME_POSITION;
  return BOARD_I2C_OK;
}

board_i2c_status_t board_oled_start(const uint8_t *frame)
{
  if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U)) return BOARD_I2C_CONTEXT_ERROR;
  if (frame == 0) return BOARD_I2C_INVALID_ARGUMENT;
  if ((status.state != BOARD_OLED_OFF) && (status.state != BOARD_OLED_READY) &&
      (status.state != BOARD_OLED_FAULT)) return BOARD_I2C_BUS_BUSY;
  image = frame;
  active_plan = 0;
  panel_on = 0U;
  memset(&status, 0, sizeof(status));
  status.state = BOARD_OLED_POWER_WAIT;
  state_tick = HAL_GetTick();
  return BOARD_I2C_OK;
}

board_i2c_status_t board_oled_update(const uint8_t *frame)
{
  if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U)) return BOARD_I2C_CONTEXT_ERROR;
  if (frame == 0) return BOARD_I2C_INVALID_ARGUMENT;
  if ((status.state == BOARD_OLED_OFF) || (status.state == BOARD_OLED_FAULT))
    return BOARD_I2C_NOT_READY;
  if (status.state != BOARD_OLED_READY) return BOARD_I2C_BUS_BUSY;
  image = frame;
  active_plan = 0;
  status.wire_bytes = 0U;
  status.page = 0U;
  status.column = 0U;
  status.error = BOARD_I2C_OK;
  status.failed_stage = BOARD_OLED_OFF;
  status.state = BOARD_OLED_FRAME_POSITION;
  return BOARD_I2C_OK;
}

board_i2c_status_t board_oled_submit(const uint8_t *frame, const OLED_RefreshPlan *plan)
{
  if (!plan || plan->span_count == 0U || plan->span_count > OLED_REFRESH_MAX_SPANS)
    return BOARD_I2C_INVALID_ARGUMENT;
  for (uint16_t i = 0; i < plan->span_count; i++)
    if (plan->spans[i].page >= 8U || plan->spans[i].column >= 128U ||
        plan->spans[i].length == 0U || plan->spans[i].length > 128U-plan->spans[i].column)
      return BOARD_I2C_INVALID_ARGUMENT;
  board_i2c_status_t result = board_oled_update(frame);
  if (result != BOARD_I2C_OK) return result;
  active_plan = plan; span_index = span_offset = 0U;
  status.page = plan->spans[0].page; status.column = plan->spans[0].column;
  frame_tick = HAL_GetTick();
  return BOARD_I2C_OK;
}

void board_oled_abort(board_i2c_status_t reason)
{
  if (__get_IPSR() || __get_PRIMASK()) return;
  status.failed_stage = status.state; status.error = reason;
  status.state = BOARD_OLED_FAULT; image = 0; active_plan = 0;
}

void board_oled_poll(void)
{
  if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U)) return;
  const board_oled_state_t stage = status.state;
  board_i2c_status_t result = BOARD_I2C_OK;
  switch (stage)
  {
    case BOARD_OLED_POWER_WAIT:
      if ((uint32_t)(HAL_GetTick() - state_tick) >= 100U) status.state = BOARD_OLED_SYNC;
      break;
    case BOARD_OLED_SYNC:
    {
      /* Drain a possible pending two-byte command parameter after MCU reset,
         as the external RC reset does not pulse when only MCU is reset. */
      const uint8_t packet[] = {0x00U, 0x12U, 0xAEU};
      result = send(packet, (uint16_t)sizeof(packet));
      status.state = BOARD_OLED_INIT;
      break;
    }
    case BOARD_OLED_INIT:
      result = send(init_packet, (uint16_t)sizeof(init_packet));
      state_tick = HAL_GetTick();
      status.state = BOARD_OLED_INIT_WAIT;
      break;
    case BOARD_OLED_INIT_WAIT:
      if ((uint32_t)(HAL_GetTick() - state_tick) >= 200U) status.state = BOARD_OLED_CLEAR_POSITION;
      break;
    case BOARD_OLED_CLEAR_POSITION:
    case BOARD_OLED_FRAME_POSITION:
      if ((stage == BOARD_OLED_FRAME_POSITION) && !active_plan && (status.page == 0U) && (status.column == 0U))
      { frame_tick = HAL_GetTick(); status.wire_bytes = 0U; }
      result = position((stage == BOARD_OLED_CLEAR_POSITION) ? 0U : OLED_COLUMN_OFFSET);
      status.state = (stage == BOARD_OLED_CLEAR_POSITION) ? BOARD_OLED_CLEAR_DATA : BOARD_OLED_FRAME_DATA;
      break;
    case BOARD_OLED_CLEAR_DATA: result = data_chunk(1U); break;
    case BOARD_OLED_FRAME_DATA: result = data_chunk(0U); break;
    case BOARD_OLED_DISPLAY_ON:
    {
      const uint8_t packet[] = {0x00U, 0xAFU};
      result = send(packet, (uint16_t)sizeof(packet));
      if (result == BOARD_I2C_OK) panel_on = 1U;
      status.state = BOARD_OLED_READY;
      break;
    }
    default: break;
  }
  if (result != BOARD_I2C_OK)
  {
    status.error = result;
    status.last_frame_ms = ((stage == BOARD_OLED_FRAME_POSITION) ||
                            (stage == BOARD_OLED_FRAME_DATA) ||
                            (stage == BOARD_OLED_DISPLAY_ON)) ?
                           (uint32_t)(HAL_GetTick() - frame_tick) : 0U;
    status.failed_stage = stage;
    status.state = BOARD_OLED_FAULT;
    image = 0;
  }
  else if ((status.state == BOARD_OLED_READY) && (image != 0))
  {
    status.completed_frames++;
    status.last_frame_ms = (uint32_t)(HAL_GetTick() - frame_tick);
    image = 0;
  }
}

board_oled_status_t board_oled_status(void)
{
  return status;
}
