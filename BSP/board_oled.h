#ifndef BOARD_OLED_H
#define BOARD_OLED_H

#include "board_soft_i2c.h"
#include "oled_refresh.h"
#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_OLED_ADDRESS 0x3CU
#define BOARD_OLED_WIDTH 128U
#define BOARD_OLED_HEIGHT 64U
#define BOARD_OLED_FRAME_SIZE 1024U

typedef enum
{
  BOARD_OLED_OFF = 0,
  BOARD_OLED_POWER_WAIT,
  BOARD_OLED_SYNC,
  BOARD_OLED_INIT,
  BOARD_OLED_INIT_WAIT,
  BOARD_OLED_CLEAR_POSITION,
  BOARD_OLED_CLEAR_DATA,
  BOARD_OLED_FRAME_POSITION,
  BOARD_OLED_FRAME_DATA,
  BOARD_OLED_DISPLAY_ON,
  BOARD_OLED_READY,
  BOARD_OLED_FAULT
} board_oled_state_t;

typedef struct
{
  board_oled_state_t state;
  board_oled_state_t failed_stage;
  board_i2c_status_t error;
  uint8_t page;
  uint8_t column;
  /* Successful frames only; reset by start, preserved by update. Duration is
     wall time from first frame-position command to completion (not drawing,
     power wait or initial RAM clear); partial-frame failures also record it. */
  uint32_t completed_frames;
  uint32_t last_frame_ms;
  uint32_t wire_bytes;
} board_oled_status_t;

/* Exclusive foreground owner on shared software I2C; initialize the bus first.
   Start borrows a const, page-major 1024-byte framebuffer until READY/FAULT.
   Caller must not change/free it while active. No I2C transfer inside start.
   Poll performs at most one small transfer (<=33 bytes); IRQs stay enabled.
   Start rejects replacement while active; restarting READY/FAULT reinitializes
   the display. No hardware reset pin on this board (external RC reset).
   This module handles SH1106 only: no drawing, logging or bus resets. */
board_i2c_status_t board_oled_start(const uint8_t *frame);
/* READY only: send a new full frame without clearing RAM, switching the panel
   off or repeating initialization. Same borrowed-buffer lifetime as start. */
board_i2c_status_t board_oled_update(const uint8_t *frame);
/* Both frame and plan remain immutable until READY/FAULT. Validated before
   starting; plan contains page-bounded spans. No buffer allocation or copy. */
board_i2c_status_t board_oled_submit(const uint8_t *frame, const OLED_RefreshPlan *plan);
/* Foreground only; synchronous soft-I2C has no reader after a call returns. */
void board_oled_abort(board_i2c_status_t reason);
void board_oled_poll(void);
board_oled_status_t board_oled_status(void);

#ifdef __cplusplus
}
#endif

#endif
