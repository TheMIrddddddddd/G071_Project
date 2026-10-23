#include "astra_port.h"
#include "hal/hal.h"
#include "u8g2_port.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace {
class Port final : public HAL {
    uint8_t *buffer_ = nullptr;
    u8g2_t *u() { return U8G2_PortContext(); }
    static int integer(float x) { return static_cast<int>(std::lround(x)); }
  public:
    void bind(uint8_t *frame) { buffer_=frame;U8G2_PortBind(frame); }
    void *_getCanvasBuffer() override { return buffer_; }
    unsigned char _getBufferTileHeight() override { return 8; }
    unsigned char _getBufferTileWidth() override { return 16; }
    void _canvasClear() override { u8g2_ClearBuffer(u()); }
    void _canvasUpdate() override {} // Publication belongs to the outer DMA app.
    void _setFont(const unsigned char *) override { u8g2_SetFont(u(),u8g2_font_5x7_tf); }
    unsigned char _getFontWidth(std::string &s) override { return static_cast<unsigned char>(std::min<unsigned>(u8g2_GetStrWidth(u(),s.c_str()),255)); }
    unsigned char _getFontHeight() override { return 7; }
    void _setDrawType(unsigned char type) override { u8g2_SetDrawColor(u(),type); }
    void _drawPixel(float x,float y) override {
        const int px=integer(x),py=integer(y);
        if(px>=0&&px<128&&py>=0&&py<64)u8g2_DrawPixel(u(),static_cast<u8g2_uint_t>(px),static_cast<u8g2_uint_t>(py));
    }
    void _drawHLine(float x,float y,float length) override {
        const int py=integer(y),a=std::max(0,integer(x)),b=std::min(128,integer(x)+integer(length));
        if(py>=0&&py<64&&b>a)u8g2_DrawHLine(u(),static_cast<u8g2_uint_t>(a),static_cast<u8g2_uint_t>(py),static_cast<u8g2_uint_t>(b-a));
    }
    void _drawVLine(float x,float y,float length) override {
        const int px=integer(x),a=std::max(0,integer(y)),b=std::min(64,integer(y)+integer(length));
        if(px>=0&&px<128&&b>a)u8g2_DrawVLine(u(),static_cast<u8g2_uint_t>(px),static_cast<u8g2_uint_t>(a),static_cast<u8g2_uint_t>(b-a));
    }
    void _drawHDottedLine(float x,float y,float length) override { for(int i=0;i<integer(length)&&i<512;i+=2)_drawPixel(x+i,y); }
    void _drawVDottedLine(float x,float y,float length) override { for(int i=0;i<integer(length)&&i<512;i+=2)_drawPixel(x,y+i); }
    void _drawEnglish(float x,float y,const std::string &s) override {
        int px=integer(x),py=integer(y);size_t skip=0;
        if(px<0){skip=static_cast<size_t>((-px+4)/5);px+=static_cast<int>(skip)*5;}
        if(skip>=s.size()||px>=128||py<=0||py>70)return;
        u8g2_DrawStr(u(),static_cast<u8g2_uint_t>(px),static_cast<u8g2_uint_t>(py),s.c_str()+skip);
    }
    void _drawChinese(float x,float y,const std::string &s) override { _drawEnglish(x,y,s); } // Demo labels are ASCII.
    void _drawBox(float x,float y,float w,float h) override {
        const int a=std::max(0,integer(x)),b=std::max(0,integer(y)),r=std::min(128,integer(x+w)),d=std::min(64,integer(y+h));
        if(r>a&&d>b)u8g2_DrawBox(u(),static_cast<u8g2_uint_t>(a),static_cast<u8g2_uint_t>(b),static_cast<u8g2_uint_t>(r-a),static_cast<u8g2_uint_t>(d-b));
    }
    void _drawFrame(float x,float y,float w,float h) override { _drawHLine(x,y,w);_drawHLine(x,y+h-1,w);_drawVLine(x,y,h);_drawVLine(x+w-1,y,h); }
    void _drawRBox(float x,float y,float w,float h,float radius) override {
        if(x<0||y<0||x+w>128||y+h>64){_drawBox(x,y,w,h);return;}
        const int iw=integer(w),ih=integer(h);if(iw<=0||ih<=0)return;
        const int r=std::clamp(integer(radius),0,std::min(iw,ih)/2);
        u8g2_DrawRBox(u(),static_cast<u8g2_uint_t>(integer(x)),static_cast<u8g2_uint_t>(integer(y)),static_cast<u8g2_uint_t>(iw),static_cast<u8g2_uint_t>(ih),static_cast<u8g2_uint_t>(r));
    }
    void _drawRFrame(float x,float y,float w,float h,float) override { _drawFrame(x,y,w,h); }
    void _drawBMP(float x,float y,float width,float height,const unsigned char *bitmap) override {
        if(!bitmap)return;
        const int w=std::clamp(integer(width),0,128),h=std::clamp(integer(height),0,64),stride=(w+7)/8;
        const int ox=integer(x),oy=integer(y),color=u8g2_GetDrawColor(u());
        if(!buffer_)return;
        // Convert coordinates once, skip invisible rows/columns, and write the
        // CPU-owned vertical-page buffer directly. This never touches DMA RAM.
        for(int py=std::max(0,-oy);py<std::min(h,64-oy);py++) {
            const int sy=oy+py;const auto mask=static_cast<uint8_t>(1U<<(sy%8));
            for(int px=std::max(0,-ox);px<std::min(w,128-ox);px++) {
                if(!(bitmap[py*stride+px/8]&(1U<<(px%8))))continue;
                auto &byte=buffer_[sy/8*128+ox+px];
                if(color==0)byte&=static_cast<uint8_t>(~mask);else if(color==1)byte|=mask;else byte^=mask;
            }
        }
    }
    void _delay(unsigned long) override { std::abort(); } // No blocking upstream paths are used.
    void _keyScan() override {}
    void _printInfo(std::string) override {} // No display ownership outside the app.
};
Port port;
}
bool AstraPort_Init(uint8_t *frame) {
    if(!frame)return false;
    if(!U8G2_PortContext()&&!U8G2_PortInit(frame))return false;
    port.bind(frame);u8g2_SetFont(U8G2_PortContext(),u8g2_font_5x7_tf);
    return HAL::inject(&port);
}
void AstraPort_Bind(uint8_t *frame) { port.bind(frame); }
