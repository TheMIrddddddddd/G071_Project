#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "app_main.h"
#include "app_bus.h"
#include "app_rgb.h"
#include "app_ui.h"
#include "board_oled.h"
#include "board_time.h"
#include "board_usart.h"
#include "showcase_model.h"
uint32_t fake_ipsr, fake_primask;
volatile board_usart3_stats_t board_usart3_stats;
static uint64_t clock_us;
static uint32_t uart_polls, oled_transactions, rgb_transactions, recoveries, ons, clears;
static uint8_t ram[8][132];
static unsigned page, column;
static bool oled_absent, rgb_absent, stuck;
static unsigned fail_data;
void test_exhaust_ui_pool(void);
uint64_t board_time_us(void){return clock_us;}
void board_time_init(void){}
uint32_t HAL_GetTick(void){return (uint32_t)(clock_us/1000U);}
uint32_t HAL_RCC_GetSysClockFreq(void){return 16000000U;}
void board_usart3_init(void){}
void board_usart3_poll(void){uart_polls++;}
uint16_t board_usart3_printf(const char *format,...)
{ char out[256];va_list args;va_start(args,format);int n=vsnprintf(out,sizeof(out),format,args);va_end(args);assert(n>=0&&n<192);return (uint16_t)n; }
board_i2c_status_t board_soft_i2c_init(void){return BOARD_I2C_OK;}
board_i2c_status_t board_soft_i2c_recover(void){recoveries++;return stuck?BOARD_I2C_SCL_TIMEOUT:BOARD_I2C_OK;}
const char *board_soft_i2c_status_string(board_i2c_status_t code)
{static const char *names[]={"OK","ADDRESS_NACK","DATA_NACK","SCL_TIMEOUT","BUS_BUSY","NOT_READY","INVALID_ARGUMENT","CONTEXT_ERROR"};return names[code];}
board_i2c_status_t board_soft_i2c_transfer(uint8_t address,const uint8_t *tx,uint16_t n,uint8_t *rx,uint16_t nr)
{
  assert(!rx&&!nr&&n>=2&&n<=33);clock_us+=(uint64_t)(n+1U)*100U;
  if(stuck)return BOARD_I2C_SCL_TIMEOUT;
  if(address==0x45){rgb_transactions++;return rgb_absent?BOARD_I2C_ADDRESS_NACK:BOARD_I2C_OK;}
  assert(address==0x3C);oled_transactions++;
  if(oled_absent)return BOARD_I2C_ADDRESS_NACK;
  if(tx[0]==0x40 && fail_data && --fail_data==0)return BOARD_I2C_DATA_NACK;
  if(tx[0]==0) {
    if(n==4&&(tx[1]&0xF8U)==0xB0U){page=tx[1]&7U;column=(tx[2]&15U)|((tx[3]&15U)<<4);}
    if(n==2&&tx[1]==0xAF)ons++;
  } else {
    assert(tx[0]==0x40&&page<8&&column+n-1U<=132);
    if(board_oled_status().state==BOARD_OLED_CLEAR_DATA)clears+=(uint32_t)n-1U;
    memcpy(&ram[page][column],tx+1,n-1U);column+=n-1U;
  }
  return BOARD_I2C_OK;
}
static void advance(uint32_t milliseconds)
{
  uint64_t until=clock_us+(uint64_t)milliseconds*1000U;
  while(clock_us<until){app_main_poll();clock_us+=100U;}
}
int main(int argc,char **argv)
{
  assert(argc==2);memset(ram,0xFF,sizeof(ram));
  if(!strcmp(argv[1],"memory"))test_exhaust_ui_pool();
  app_main_init();
  if(!strcmp(argv[1],"normal")){
    advance(304000);assert(ons==1&&clears==1056&&oled_transactions>1000);
    assert(rgb_transactions>500&&app_rgb_status()->errors==0&&recoveries==0);
    assert(board_oled_status().completed_frames>100&&uart_polls>1000);
    ShowcaseSnapshot snapshot;assert(Showcase_GetSnapshot(&snapshot));
    assert(snapshot.cycle>=3&&!app_ui_failed());
  }else if(!strcmp(argv[1],"offline")){
    oled_absent=true;advance(11000);
    assert(recoveries==0&&rgb_transactions>15&&app_bus_status()->device[0].errors>=4);
    assert(oled_transactions<12);oled_absent=false;advance(7000);assert(ons==1);
  }else if(!strcmp(argv[1],"partial")){
    advance(2000);fail_data=2;advance(2000);assert(app_bus_status()->device[0].errors>=1);
    advance(3000);assert(ons>=2&&clears>=2112&&!app_bus_status()->blocked);
  }else if(!strcmp(argv[1],"stuck")){
    advance(2000);stuck=true;unsigned before=recoveries;advance(2300);
    assert(recoveries-before==3&&app_bus_status()->blocked);unsigned writes=rgb_transactions;
    advance(3000);assert(recoveries-before==3&&rgb_transactions==writes);
    stuck=false;advance(3000);assert(!app_bus_status()->blocked&&rgb_transactions>writes&&ons>=2);
  }else if(!strcmp(argv[1],"rgb")){
    advance(1000);unsigned before=rgb_transactions;
    assert(app_rgb_set(APP_RGB_FLASH,4));advance(2500);assert(rgb_transactions==before+1);
    assert(app_rgb_set(APP_RGB_BREATH,7));advance(2500);assert(rgb_transactions==before+2);
    assert(!app_rgb_set(APP_RGB_FIXED,0x80));
  }else if(!strcmp(argv[1],"memory")){
    assert(app_ui_failed());advance(5000);assert(ons==1&&rgb_transactions>5&&uart_polls>100);
  }else if(!strcmp(argv[1],"rgb_offline")){
    rgb_absent=true;advance(11000);assert(ons==1&&oled_transactions>100&&recoveries==0);
    rgb_absent=false;advance(7000);assert(app_rgb_status()->valid&&app_rgb_status()->writes>1);
  }else assert(false);
  printf("PASS app %s: polls=%lu OLED=%lu RGB=%lu recover=%lu on=%lu clear=%lu\n",argv[1],
    (unsigned long)uart_polls,(unsigned long)oled_transactions,(unsigned long)rgb_transactions,
    (unsigned long)recoveries,(unsigned long)ons,(unsigned long)clears);
}
