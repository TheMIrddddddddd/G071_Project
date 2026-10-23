#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board_oled.h"

uint32_t fake_ipsr, fake_primask;
static uint32_t tick;
static unsigned int calls, commands, clear_bytes, frame_bytes, display_on;
static uint8_t frame[BOARD_OLED_FRAME_SIZE];
static uint8_t ram[8][132];
static unsigned int page, column, failure_call, positions;
static unsigned int wire_bytes;

uint32_t HAL_GetTick(void) { return tick; }
board_i2c_status_t board_soft_i2c_transfer(uint8_t address,
                                         const uint8_t *tx, uint16_t length,
                                         uint8_t *rx, uint16_t rx_length)
{
  assert(address == 0x3C && tx && length >= 2 && length <= 33);
  assert(!rx && !rx_length);
  calls++;
  wire_bytes += (unsigned int)length + 1U;
  if (calls == failure_call) return BOARD_I2C_DATA_NACK;
  if (tx[0] == 0)
  {
    commands++;
    if (length == 4 && (tx[1] & 0xF8U) == 0xB0U)
    {
      positions++;
      page = tx[1] & 7U;
      column = (unsigned int)(tx[2] & 15U) | ((unsigned int)(tx[3] & 15U) << 4);
      assert(column < 132);
    }
    if (length == 2 && tx[1] == 0xAF) { display_on++; assert(frame_bytes == 1024); }
  }
  else
  {
    assert(tx[0] == 0x40);
    assert(column + length - 1U <= 132U && page < 8);
    if (board_oled_status().state == BOARD_OLED_CLEAR_DATA)
    {
      for (uint16_t i = 1; i < length; i++) assert(tx[i] == 0);
      clear_bytes += length - 1U;
    }
    else { frame_bytes += length - 1U; }
    memcpy(&ram[page][column], tx + 1, length - 1U);
    column = (column + length - 1U) % 132U; /* SH1106 auto-increment, page unchanged. */
  }
  return BOARD_I2C_OK;
}

static void run_to_ready(void)
{
  for (unsigned int i = 0; i < 1000 && board_oled_status().state != BOARD_OLED_READY; i++)
  {
    const unsigned int old_calls = calls;
    board_oled_poll(); tick++;
    assert(calls - old_calls <= 1U);
    assert(board_oled_status().state != BOARD_OLED_FAULT);
  }
  assert(board_oled_status().state == BOARD_OLED_READY);
}

int main(void)
{
  for (unsigned int i = 0; i < sizeof(frame); i++) frame[i] = (uint8_t)(i ^ (i >> 3));
  memset(ram, 0xFF, sizeof(ram));
  assert(board_oled_start(NULL) == BOARD_I2C_INVALID_ARGUMENT);
  assert(board_oled_start(frame) == BOARD_I2C_OK);
  assert(board_oled_start(frame) == BOARD_I2C_BUS_BUSY);
  board_oled_poll(); assert(calls == 0);
  tick = 99; board_oled_poll(); assert(calls == 0);
  run_to_ready();
  assert(clear_bytes == 8U * 132U && frame_bytes == 1024 && display_on == 1);
  assert(positions == 16U); /* One position per clear/frame page. */
  assert(board_oled_status().wire_bytes == 1131U); /* Full frame plus display-on. */
  for (unsigned int p = 0; p < 8; p++)
  {
    assert(ram[p][0] == 0 && ram[p][1] == 0 && ram[p][130] == 0 && ram[p][131] == 0);
    assert(memcmp(&ram[p][2], &frame[p * 128U], 128) == 0);
  }
  unsigned int before = calls;
  for (unsigned int i = 0; i < 50; i++) board_oled_poll();
  assert(calls == before);
  /* Updates must not reinitialize, clear or power-cycle the display. */
  assert(board_oled_status().completed_frames == 1U);
  assert(board_oled_update(NULL) == BOARD_I2C_INVALID_ARGUMENT);
  for (unsigned int i = 0; i < sizeof(frame); i++) frame[i] ^= 0xFFU;
  const unsigned int old_clear = clear_bytes, old_on = display_on;
  const unsigned int old_calls = calls;
  const unsigned int old_positions = positions, old_wire = wire_bytes;
  assert(board_oled_update(frame) == BOARD_I2C_OK);
  assert(board_oled_update(frame) == BOARD_I2C_BUS_BUSY);
  assert(board_oled_start(frame) == BOARD_I2C_BUS_BUSY);
  run_to_ready();
  assert(calls - old_calls == 40U);
  assert(positions - old_positions == 8U && wire_bytes - old_wire == 1128U);
  assert(board_oled_status().wire_bytes == 1128U);
  assert(clear_bytes == old_clear && display_on == old_on);
  assert(board_oled_status().completed_frames == 2U);
  assert(board_oled_status().last_frame_ms == 39U);
  for (unsigned int p = 0; p < 8; p++)
    assert(memcmp(&ram[p][2], &frame[p * 128U], 128) == 0);
  tick = UINT32_MAX - 30U;
  assert(board_oled_update(frame) == BOARD_I2C_OK);
  run_to_ready();
  assert(board_oled_status().last_frame_ms == 39U);
  assert(board_oled_status().completed_frames == 3U);
  OLED_RefreshPlan region={0};region.span_count=1;
  region.spans[0].page=2U;region.spans[0].column=17U;region.spans[0].length=40U;
  region.wire_bytes=49U;
  memset(ram,0xA5,sizeof(ram));
  before=positions;
  assert(board_oled_submit(frame,&region)==BOARD_I2C_OK);
  run_to_ready();assert(board_oled_status().wire_bytes==49U);
  assert(positions-before==1U);
  for(unsigned p=0;p<8U;p++) for(unsigned c=0;c<132U;c++)
    assert(ram[p][c]==((p==2U&&c>=19U&&c<59U)?frame[p*128U+c-2U]:0xA5U));
  assert(board_oled_status().completed_frames==4U);
  region.spans[0].column=255U;region.spans[0].length=65282U;
  assert(board_oled_submit(frame,&region)==BOARD_I2C_INVALID_ARGUMENT);
  region.spans[0].column=127U;region.spans[0].length=2U;
  assert(board_oled_submit(frame,&region)==BOARD_I2C_INVALID_ARGUMENT);
  memset(ram,0xA5,sizeof(ram));
  region.span_count=3U;
  region.spans[0]=(OLED_RefreshSpan){1U,3U,65U};
  region.spans[1]=(OLED_RefreshSpan){1U,100U,28U};
  region.spans[2]=(OLED_RefreshSpan){7U,95U,33U};
  before=positions; unsigned int transaction_start=calls;
  assert(board_oled_submit(frame,&region)==BOARD_I2C_OK);
  run_to_ready();
  assert(positions-before==3U && calls-transaction_start==9U);
  assert(board_oled_status().wire_bytes==153U);
  for(unsigned p=0;p<8U;p++) for(unsigned c=0;c<132U;c++) {
    int written=(p==1U&&((c>=5U&&c<70U)||(c>=102U&&c<130U)))
               ||(p==7U&&c>=97U&&c<130U);
    assert(ram[p][c]==(written?frame[p*128U+c-2U]:0xA5U));
  }
  /* Failed dynamic frames stop; no success count and no automatic init. */
  assert(board_oled_update(frame) == BOARD_I2C_OK);
  failure_call = calls + 3U; /* Fail second chunk after successful first chunk. */
  board_oled_poll(); board_oled_poll(); board_oled_poll();
  assert(board_oled_status().state == BOARD_OLED_FAULT);
  assert(board_oled_status().failed_stage == BOARD_OLED_FRAME_DATA);
  assert(board_oled_status().completed_frames == 5U);
  assert(board_oled_status().column == 32U);
  before=calls; board_oled_poll(); assert(calls==before);
  assert(board_oled_update(frame) == BOARD_I2C_NOT_READY);
  failure_call = 0;
  /* Failure stops transfers and preserves stage for diagnostics. */
  assert(board_oled_start(frame) == BOARD_I2C_OK);
  tick += 100; board_oled_poll();
  failure_call = calls + 1U;
  board_oled_poll();
  assert(board_oled_status().state == BOARD_OLED_FAULT);
  assert(board_oled_status().failed_stage == BOARD_OLED_SYNC);
  assert(board_oled_status().error == BOARD_I2C_DATA_NACK);
  before = calls; board_oled_poll(); assert(calls == before);
  failure_call = 0; clear_bytes = frame_bytes = display_on = 0;
  tick = UINT32_MAX - 30U;
  assert(board_oled_start(frame) == BOARD_I2C_OK);
  run_to_ready();
  for(unsigned int p=0;p<8U;p++)
    assert(memcmp(&ram[p][2], &frame[p*128U],128U)==0);
  const unsigned int fail_offsets[] = {2U, 3U, 4U, 51U, 52U, 91U};
  const board_oled_state_t fail_stages[] = {
    BOARD_OLED_INIT, BOARD_OLED_CLEAR_POSITION, BOARD_OLED_CLEAR_DATA,
    BOARD_OLED_FRAME_POSITION, BOARD_OLED_FRAME_DATA, BOARD_OLED_DISPLAY_ON
  };
  for (unsigned int j = 0; j < sizeof(fail_offsets) / sizeof(fail_offsets[0]); j++)
  {
    clear_bytes = frame_bytes = display_on = 0;
    failure_call = calls + fail_offsets[j];
    assert(board_oled_start(frame) == BOARD_I2C_OK);
    for (unsigned int i = 0; i < 1000 && board_oled_status().state != BOARD_OLED_FAULT; i++)
    { board_oled_poll(); tick++; }
    assert(board_oled_status().state == BOARD_OLED_FAULT);
    assert(board_oled_status().failed_stage == fail_stages[j]);
    before = calls; board_oled_poll(); assert(calls == before);
  }
  fake_ipsr = 1;
  assert(board_oled_start(frame) == BOARD_I2C_CONTEXT_ERROR);
  assert(board_oled_update(frame) == BOARD_I2C_CONTEXT_ERROR);
  fake_ipsr = 0; fake_primask = 1;
  assert(board_oled_start(frame) == BOARD_I2C_CONTEXT_ERROR);
  assert(board_oled_update(frame) == BOARD_I2C_CONTEXT_ERROR);
  puts("PASS: OLED single-position/chunk continuation, full frame 1128 bytes/40 transactions, region mapping/gap preservation, failure/reinit, one transaction/poll, timing/wrap, context");
  return 0;
}
