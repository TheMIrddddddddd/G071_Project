#include "app_display.h"
#include "app_ui.h"
#include "app_rgb.h"
#include "app_bus.h"
#include "board_oled.h"
#include "board_usart.h"
#include "board_time.h"
#include "oled_refresh.h"
#include <string.h>
static OLED_Refresh refresh;
static uint32_t generation, sequence, completed, last_ms, last_wire, skipped;
static uint32_t max_draw_us, max_model_us;
static uint16_t last_changed, last_pixels, last_spans;
static uint64_t last_draw, report_tick;
static bool restart, ui_error_reported;
static uint32_t duration(uint64_t start)
{ uint64_t value=board_time_us()-start;return value>UINT32_MAX?UINT32_MAX:(uint32_t)value; }
void app_display_init(uint64_t now)
{
  OLED_RefreshInit(&refresh,OLED_REFRESH_DIFF);restart=true;
  generation=app_bus_status()->generation; report_tick=now;last_draw=now;
  uint8_t *bootstrap=0;(void)OLED_RefreshBeginDraw(&refresh,&bootstrap);
  bool ok=app_ui_init(now,bootstrap);(void)OLED_RefreshCancelDraw(&refresh);
  (void)board_usart3_printf("[UI] mode=%s init=%s heap_peak=%lu pool=2048\r\n",
#if APP_LAYOUT
    "LAYOUT",
#elif APP_SHOWCASE
    "SHOWCASE",
#else
    "STATUS",
#endif
    ok?"OK":"FAIL",(unsigned long)app_ui_heap_peak());
}
static void invalidate(void)
{
  board_oled_abort(BOARD_I2C_BUS_BUSY);
  if(OLED_RefreshIsActive(&refresh)) (void)OLED_RefreshComplete(&refresh,false);
  else (void)OLED_RefreshInvalidate(&refresh);
  restart=true;
}
void app_display_poll(uint64_t now)
{
  uint32_t ms=(uint32_t)(now/1000U);
  if(generation!=app_bus_status()->generation)
  {generation=app_bus_status()->generation;invalidate();}
  uint64_t begin=board_time_us();(void)app_ui_update(now);
  uint32_t cost=duration(begin);if(cost>max_model_us)max_model_us=cost;
  if(app_ui_failed()&&!ui_error_reported)
  {ui_error_reported=true;(void)board_usart3_printf("[UI] initialization/allocation failed; fallback status, RGB/UART remain active\r\n");}
  /* Model time is independent of transfers. Select raster cadence by UI mode;
     overwrite the pending slot with the newest complete snapshot, never active. */
  uint64_t period=APP_LAYOUT?40000U:(APP_SHOWCASE?33333U:1000000U);
  if(!sequence||now-last_draw>=period)
  {
    last_draw=now;uint8_t *draw=0;
    if(OLED_RefreshBeginDraw(&refresh,&draw)==OLED_REFRESH_OK)
    {
      app_ui_status_t ui={(uint32_t)(now/1000000U),completed,last_ms,
        app_bus_status()->device[0].errors+app_bus_status()->device[1].errors,app_rgb_color_name()};
      begin=board_time_us();bool ok=app_ui_draw(draw,&ui);cost=duration(begin);
      if(cost>max_draw_us)max_draw_us=cost;
      if(ok) {
        OLED_FrameMeta meta={0};meta.seq=++sequence;meta.sample_us=now;
        meta.publish_us=board_time_us();meta.draw_us=cost;
        (void)OLED_RefreshPublish(&refresh,&meta);
      } else (void)OLED_RefreshCancelDraw(&refresh);
    }
  }
  if(!app_bus_status()->blocked)
  {
    board_oled_poll();board_oled_status_t state=board_oled_status();
    if(refresh.active&&state.state==BOARD_OLED_READY)
    {
      (void)OLED_RefreshComplete(&refresh,true);completed++;
      last_ms=state.last_frame_ms;last_wire=state.wire_bytes;restart=false;
      last_changed=refresh.plan.changed_bytes;last_pixels=refresh.plan.image_bytes;last_spans=refresh.plan.span_count;
      app_bus_success(APP_DEVICE_OLED);
    }
    else if(refresh.active&&state.state==BOARD_OLED_FAULT)
    {
      (void)OLED_RefreshComplete(&refresh,false);restart=true;
      (void)board_usart3_printf("[OLED] FAIL stage=%u page=%u col=%u error=%s\r\n",
        (unsigned)state.failed_stage,(unsigned)state.page,(unsigned)state.column,board_soft_i2c_status_string(state.error));
      app_bus_failure(APP_DEVICE_OLED,state.error,ms);
    }
    if(!refresh.active&&refresh.pending&&app_bus_can_try(APP_DEVICE_OLED,ms))
    {
      OLED_RefreshResult selected=OLED_RefreshStart(&refresh);
      if(selected==OLED_REFRESH_OK)
      {
        const uint8_t *image=OLED_RefreshActiveImage(&refresh);
        board_i2c_status_t result=restart?board_oled_start(image):board_oled_submit(image,OLED_RefreshGetPlan(&refresh));
        if(result!=BOARD_I2C_OK)
        {(void)OLED_RefreshComplete(&refresh,false);restart=true;app_bus_failure(APP_DEVICE_OLED,result,ms);}
      }
      else if(selected==OLED_REFRESH_NO_CHANGE)skipped++;
    }
  }
  if(now-report_tick>=5000000U)
  {
    report_tick=now;
    (void)board_usart3_printf("[DISPLAY] frames=%lu frame_ms=%lu wire=%lu unchanged=%lu replaced=%lu\r\n",
      (unsigned long)completed,(unsigned long)last_ms,(unsigned long)last_wire,(unsigned long)skipped,(unsigned long)refresh.replaced);
    (void)board_usart3_printf("[DIFF] changed=%u pixels=%u spans=%u OLED_err=%lu RGB_err=%lu recover_fail=%lu\r\n",
      (unsigned)last_changed,(unsigned)last_pixels,(unsigned)last_spans,
      (unsigned long)app_bus_status()->device[0].errors,(unsigned long)app_bus_status()->device[1].errors,
      (unsigned long)app_bus_status()->recovery_failures);
    (void)board_usart3_printf("[PERF] model_max_us=%lu draw_max_us=%lu rgb_late_ms=%lu tx_drop=%lu heap_peak=%lu\r\n",
      (unsigned long)max_model_us,(unsigned long)max_draw_us,(unsigned long)app_rgb_status()->max_late_ms,
      (unsigned long)board_usart3_stats.tx_dropped_messages,(unsigned long)app_ui_heap_peak());
  }
}
