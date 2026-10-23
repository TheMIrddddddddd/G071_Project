#include "showcase_model.h"
#include "smooth_memory.h"
#include "astra/config/config.h"
#include "astra_port.h"
#include "hal/hal.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while(0)
static uint64_t now;
static std::array<uint8_t,1024> frame;
static ShowcaseSnapshot snapshot() { ShowcaseSnapshot s{}; CHECK(Showcase_GetSnapshot(&s));return s; }
static void advance(uint64_t until) { while(now<until) { now+=1000;CHECK(Showcase_Update(now)); } }
static void dump(FILE *f) {
    CHECK(Showcase_Draw(frame.data()));std::fputs("P5\n128 64\n255\n",f);
    for(unsigned y=0;y<64;y++) for(unsigned x=0;x<128;x++)
        CHECK(std::fputc((frame[y/8*128+x]&(1U<<(y%8)))?255:0,f)!=EOF);
}
int main(int argc,char **argv) {
    CHECK(argc>=2);const char *which=argv[1];
    if(!std::strcmp(which,"guards")) {
        CHECK(!Showcase_Update(0));CHECK(!Showcase_Draw(nullptr));CHECK(!Showcase_GetSnapshot(nullptr));
    }
    CHECK(Showcase_Init(0));CHECK(Showcase_Draw(frame.data()));
    SmoothMemory_Seal();const auto mem=SmoothMemory_GetStats();
    if(!std::strcmp(which,"bitmap")) {
        std::array<uint8_t,1040> guarded;std::array<uint8_t,1024> expected;
        std::array<uint8_t,120> bitmap{};
        for(unsigned i=0;i<bitmap.size();i++)bitmap[i]=static_cast<uint8_t>(i*73+19);
        for(int color=0;color<3;color++)for(int x=-32;x<=130;x+=9)for(int y=-32;y<=66;y+=11) {
            guarded.fill(0xA5);expected.fill(0xA5);AstraPort_Bind(guarded.data()+8);HAL::setDrawType(static_cast<uint8_t>(color));
            HAL::drawBMP(static_cast<float>(x),static_cast<float>(y),30,30,bitmap.data());
            for(int py=0;py<30;py++)for(int px=0;px<30;px++) {
                const int sx=x+px,sy=y+py;
                if(sx<0||sx>=128||sy<0||sy>=64||!(bitmap[py*4+px/8]&(1U<<(px%8))))continue;
                auto &pixel=expected[static_cast<size_t>(sy/8*128+sx)];const auto mask=static_cast<uint8_t>(1U<<(sy%8));
                if(color==0)pixel&=static_cast<uint8_t>(~mask);else if(color==1)pixel|=mask;else pixel^=mask;
            }
            CHECK(!std::memcmp(guarded.data()+8,expected.data(),expected.size()));
            for(unsigned i=0;i<8;i++)CHECK(guarded[i]==0xA5&&guarded[1032+i]==0xA5);
        }
    } else if(!std::strcmp(which,"order")) {
        CHECK(snapshot().section==SHOWCASE_ASTRA);
        advance(SHOWCASE_MENU_US-1000);CHECK(snapshot().section==SHOWCASE_ASTRA);
        advance(SHOWCASE_MENU_US);CHECK(snapshot().section==SHOWCASE_STICKS);CHECK(snapshot().algorithm==0);
        advance(SHOWCASE_CYCLE_US);CHECK(snapshot().section==SHOWCASE_ASTRA);CHECK(snapshot().cycle==1);
    } else if(!std::strcmp(which,"decoupled")) {
        const auto before=snapshot();advance(80000);
        CHECK(snapshot().updates==6);CHECK(snapshot().renders==before.renders);
        const auto at=snapshot();CHECK(Showcase_Draw(frame.data()));
        CHECK(snapshot().renders==before.renders+1);
        for(unsigned i=0;i<5;i++)CHECK(Showcase_Draw(frame.data()));
        const auto after=snapshot();CHECK(after.renders==before.renders+1);
        CHECK(after.camera_x==at.camera_x&&after.selector_w==at.selector_w);
    } else if(!std::strcmp(which,"cycle") || !std::strcmp(which,"allocation")) {
        unsigned algorithms=0,sections=0;uint64_t until=SHOWCASE_CYCLE_US*2+1000000;
        while(now<until) { advance(now+1000);auto s=snapshot();sections|=1U<<s.section;if(s.section==SHOWCASE_STICKS)algorithms|=1U<<s.algorithm;
            for(float value:s.current)CHECK(std::isfinite(value));
            if(now%80000==0)CHECK(Showcase_Draw(frame.data())); }
        CHECK(algorithms==63);CHECK(sections==3);CHECK(snapshot().cycle==2);
    } else if(!std::strcmp(which,"spring")) {
        advance(SHOWCASE_MENU_US+1000000);const auto s=snapshot();
        // First target begins at 800ms. This must be the actual library critical
        // spring k=100,c=20,m=1, rather than a linear/exponential placeholder.
        const float t=.2f,expected=14*(1-std::exp(-10*t)*(1+10*t));
        CHECK(std::fabs(s.current[0]-expected)<.025f);CHECK(s.target[0]==14);
    } else if(!std::strcmp(which,"retarget")) {
        advance(SHOWCASE_MENU_US+1100000);const auto a=snapshot();advance(SHOWCASE_MENU_US+1120000);const auto b=snapshot();
        CHECK(b.target[0]<0);CHECK(std::fabs(b.current[0]-a.current[0])<2); // No teleport on reversal.
        advance(SHOWCASE_MENU_US+5200000);const auto c=snapshot();CHECK(std::fabs(c.current[0])<.1f);
    } else if(!std::strcmp(which,"menu")) {
        advance(15*SHOWCASE_MENU_ACTION_US+500000);
        auto s=snapshot();CHECK(s.section==SHOWCASE_ASTRA);CHECK(s.menu_page==5);CHECK(s.menu_index==9);CHECK(s.camera_y<0);
        advance(17*SHOWCASE_MENU_ACTION_US+500000);
        s=snapshot();CHECK(s.menu_page==5&&s.menu_index==9); // Return + reopen preserves selection.
        CHECK(s.menu_action==17);CHECK(std::fabs(s.selector_w)>0);
        advance(18*SHOWCASE_MENU_ACTION_US+500000);CHECK(snapshot().popup);
    } else if(!std::strcmp(which,"cadence")) {
        CHECK(astra::getUIConfig().cameraAnimationSpeed==73);
        CHECK(astra::getUIConfig().selectorXAnimationSpeed==73);
        CHECK(astra::getUIConfig().selectorWidthAnimationSpeed==73);
        advance(1000000);CHECK(snapshot().updates==75);
        advance(2000000);CHECK(snapshot().updates==150);CHECK(snapshot().skipped_ticks==0);
    } else if(!std::strcmp(which,"guards")) {
        advance(2000000);const auto a=snapshot();CHECK(!Showcase_Update(now-1));CHECK(snapshot().updates==a.updates);
        CHECK(Showcase_Update(UINT64_C(864000000000)));const auto b=snapshot();CHECK(b.skipped_ticks>0);
        for(float v:b.current) CHECK(std::isfinite(v));
        CHECK(Showcase_Draw(frame.data()));CHECK(!Showcase_Draw(nullptr));
    } else if(!std::strcmp(which,"preview")) {
        if(argc>2) {
            FILE *f=std::fopen(argv[2],"wb");CHECK(f);dump(f);
            for(uint64_t t=80000;t<=SHOWCASE_CYCLE_US;t+=80000){advance(t);dump(f);}CHECK(std::fclose(f)==0);
        } else { advance(2300000);CHECK(Showcase_Draw(frame.data()));unsigned on=0;for(auto b:frame)if(b)++on;CHECK(on>50); }
    } else CHECK(false);
    const auto after=SmoothMemory_GetStats();CHECK(after.allocations==mem.allocations);CHECK(after.frees==mem.frees);
    std::printf("PASS showcase_%s alloc=%lu peak=%lu live=%lu\n",which,(unsigned long)after.allocations,(unsigned long)after.peak_bytes,(unsigned long)after.live_bytes);
}
