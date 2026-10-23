#include "app_ui.h"
#include "u8g2_port.h"
#include <cstdio>
#if APP_SHOWCASE
#include "showcase_model.h"
#include "smooth_memory.h"
#endif
namespace { bool failed; }
bool app_ui_init(uint64_t now, uint8_t *bootstrap)
{
  U8G2_PortInit(bootstrap);
#if APP_SHOWCASE
  try {
    if(!Showcase_InitWithBuffer(now,bootstrap)) {failed=true;return false;}
    SmoothMemory_Seal();return true;
  } catch(...) {failed=true;return false;}
#else
  (void)now; return true;
#endif
}
bool app_ui_failed(void) { return failed; }
uint32_t app_ui_heap_peak(void)
{
#if APP_SHOWCASE
  return SmoothMemory_GetStats().peak_bytes;
#else
  return 0;
#endif
}
bool app_ui_update(uint64_t now)
{
#if APP_SHOWCASE
  if(failed)return false;
  try { return Showcase_Update(now); } catch(...) {failed=true;return false;}
#else
  (void)now;return true;
#endif
}
bool app_ui_draw(uint8_t *frame,const app_ui_status_t *status)
{
#if APP_SHOWCASE
  if(!failed) {
    try {return Showcase_Draw(frame);} catch(...) {failed=true;}
  }
#endif
  auto *u=U8G2_PortContext();U8G2_PortBind(frame);u8g2_ClearBuffer(u);
  u8g2_DrawFrame(u,0,0,128,64);u8g2_DrawStr(u,5,10,failed?"UI MEMORY ERROR":"STM32G071 STATUS");
  char line[32];
  std::snprintf(line,sizeof(line),"RUN %lu RGB %s",static_cast<unsigned long>(status->uptime),status->rgb);
  u8g2_DrawStr(u,5,23,line);
  std::snprintf(line,sizeof(line),"FRAME %lu %lums",static_cast<unsigned long>(status->frames),static_cast<unsigned long>(status->last_ms));
  u8g2_DrawStr(u,5,36,line);
  std::snprintf(line,sizeof(line),"I2C ERR %lu",static_cast<unsigned long>(status->errors));u8g2_DrawStr(u,5,49,line);
  u8g2_DrawStr(u,5,60,"PB5 SCL / PB6 SDA");return true;
}
