#include "showcase_model.h"
#include "astra_port.h"
#include "astra/ui/item/camera/camera.h"
#include "core/animation/generators/spring/spring.hpp"
#include "core/animation/generators/easing/easing.hpp"
#include "u8g2_port.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <new>
namespace {
using smooth_ui_toolkit::Spring;
using smooth_ui_toolkit::Easing;
struct Gesture { uint32_t us;std::array<float,6> targets; };
constexpr Gesture gestures[]={
 {0,{0,0,0,0,0,0}}, {800000,{14,0,-14,0,14,-14}},
 {1100000,{-14,0,14,0,-14,14}}, {2200000,{8,-8,-8,8,6,-6}},
 {3500000,{-8,8,8,-8,-6,6}}, {4400000,{0,0,0,0,0,0}},
 {6400000,{0,-14,0,14,10,-10}}, {6700000,{0,14,0,-14,-10,10}},
 {7900000,{5,5,-5,-5,3,-3}}, {9600000,{0,0,0,0,0,0}}
};
struct Model {
    Spring spring[6];Easing easing[6];uint64_t starts[6]{};
    astra::Tile home;
    std::array<astra::List,34> nodes;
    astra::Camera camera;
    astra::Selector selector;
    astra::Menu *menu=nullptr;
    std::array<uint8_t,1024> frame{};
    ShowcaseSnapshot out{};
    uint64_t epoch=0,last_us=0,last_tick=0;
    uint32_t previous_cycle=UINT32_MAX;
    int previous_stage=-1,action=-1;
    bool popup=false,popup_closing=false;
    bool dirty=true;
    float popup_y=-30;
    Model() {
        home.title="ASTRA DEMO";home.childMenu.reserve(4);
        const char *titles[]={"CONTROL","STATUS","SYSTEM","ABOUT","Overview","Diagnostics","Input monitor","Bus state","Battery",
            "System clock","I2C display","I2C input","DMA transfer","Frame counter","Input sample","Calibration","Error counter","Bus recovery","Memory usage","Uptime","Build version",
            "Display","Sound","Power","Info","Brightness","Theme","Screen timeout","Animation","Input","Calibration","Output mapping","Astra UI","Demo version"};
        for(size_t i=0;i<nodes.size();i++)nodes[i].title=titles[i];
        for(unsigned i=0;i<4;i++)home.addItem(&nodes[i]);
        for(unsigned i=4;i<=8;i++)nodes[1].addItem(&nodes[i]);
        for(unsigned i=9;i<=20;i++)nodes[5].addItem(&nodes[i]);
        for(unsigned i=21;i<=24;i++)nodes[2].addItem(&nodes[i]);
        for(unsigned i=25;i<=28;i++)nodes[21].addItem(&nodes[i]);
        for(unsigned i=29;i<=31;i++)nodes[0].addItem(&nodes[i]);
        for(unsigned i=32;i<=33;i++)nodes[3].addItem(&nodes[i]);
        // Four explicit XBM icons, 30x30, 4 bytes per row. Text-only nodes have no bitmap.
        for(unsigned icon=0;icon<4;icon++) {
            auto &bits=nodes[icon].pic;bits.assign(120,0);
            for(int y=0;y<30;y++)for(int x=0;x<30;x++) {
                const int dx=x-15,dy=y-15;
                bool on=false;
                if(icon==0)on=(dx*dx+dy*dy>=120&&dx*dx+dy*dy<=155)||(std::abs(dx)<=1&&std::abs(dy)<=8)||(std::abs(dy)<=1&&std::abs(dx)<=8);
                else if(icon==1)on=(x>=5&&x<=8&&y>=17&&y<=25)||(x>=12&&x<=15&&y>=10&&y<=25)||(x>=19&&x<=22&&y>=4&&y<=25);
                else if(icon==2)on=(dx*dx+dy*dy>=64&&dx*dx+dy*dy<=100)||(std::abs(dx)<=2&&std::abs(dy)<=13)||(std::abs(dy)<=2&&std::abs(dx)<=13);
                else on=(dx*dx+dy*dy>=130&&dx*dx+dy*dy<=165)||(std::abs(dx)<=1&&y>=14&&y<=22)||(std::abs(dx)<=1&&y>=7&&y<=9);
                if(on)bits[static_cast<size_t>(y*4+x/8)]|=static_cast<uint8_t>(1U<<(x%8));
            }
        }
    }
};
alignas(Model) unsigned char storage[sizeof(Model)];
Model *model=nullptr;
uint64_t tickTime(uint64_t tick) { return tick/75U*1000000U+(tick%75U)*1000000U/75U; }
void bind() { AstraPort_Bind(model->frame.data());u8g2_SetFont(U8G2_PortContext(),u8g2_font_5x7_tf);u8g2_SetDrawColor(U8G2_PortContext(),1); }
void sampleChannels(uint64_t local_us) {
    for(unsigned i=0;i<6;i++) {
        const float t=static_cast<float>(local_us-model->starts[i])*1e-6f;
        if(model->out.algorithm<3){model->spring[i].next(t);model->out.current[i]=model->spring[i].value;}
        else{model->easing[i].next(t);model->out.current[i]=model->easing[i].value;}
    }
}
void resetChannels(unsigned algorithm) {
    model->out.algorithm=static_cast<uint8_t>(algorithm);model->out.gesture=0;
    for(unsigned i=0;i<6;i++) {
        auto &s=model->spring[i];s.start=0;s.end=0;s.springOptions={};
        s.springOptions.stiffness=algorithm==1?64.0f:100.0f;
        s.springOptions.damping=algorithm==0?20.0f:algorithm==1?10.0f:30.0f;
        s.springOptions.restDelta=.02f;s.springOptions.restSpeed=.02f;s.init();
        auto &e=model->easing[i];e.start=0;e.end=0;e.easingOptions.duration=.65f;
        e.easingOptions.easingFunction=algorithm==3?smooth_ui_toolkit::ease::ease_out_quad:
            algorithm==4?smooth_ui_toolkit::ease::ease_out_expo:smooth_ui_toolkit::ease::ease_in_out_cubic;e.init();
        model->starts[i]=0;model->out.current[i]=0;model->out.target[i]=0;
    }
}
void retarget(unsigned gesture,uint64_t local_us) {
    for(unsigned i=0;i<6;i++) {
        const float target=gestures[gesture].targets[i];
        if(target==model->out.target[i])continue;
        if(model->out.algorithm<3)model->spring[i].retarget(model->spring[i].value,target);
        else model->easing[i].retarget(model->easing[i].value,target);
        model->starts[i]=local_us;model->out.target[i]=target;
    }
    model->out.gesture=static_cast<uint8_t>(gesture);
}
void drawSticks() {
    auto *u=U8G2_PortContext();u8g2_ClearBuffer(u);
    u8g2_DrawStr(u,1,8,Showcase_AlgorithmName(model->out.algorithm));
    const unsigned g=model->out.gesture;
    u8g2_DrawStr(u,88,8,g==0||g==5||g==9?"HOLD":g==2||g==7?"REVERSE":"PUSH");
    u8g2_DrawHLine(u,0,12,128);
    for(unsigned stick=0;stick<2;stick++) {
        const int cx=stick==0?25:103,cy=40;
        u8g2_DrawCircle(u,static_cast<u8g2_uint_t>(cx),cy,18,U8G2_DRAW_ALL);
        u8g2_DrawHLine(u,static_cast<u8g2_uint_t>(cx-2),cy,5);u8g2_DrawVLine(u,static_cast<u8g2_uint_t>(cx),cy-2,5);
        float x=model->out.current[stick*2],y=model->out.current[stick*2+1];
        const float len=std::hypot(x,y);if(len>14){x*=14/len;y*=14/len;}
        const int px=cx+static_cast<int>(std::lround(x)),py=cy+static_cast<int>(std::lround(y));
        u8g2_DrawDisc(u,static_cast<u8g2_uint_t>(px),static_cast<u8g2_uint_t>(py),3,U8G2_DRAW_ALL);
        const int tx=cx+static_cast<int>(model->out.target[stick*2]),ty=cy+static_cast<int>(model->out.target[stick*2+1]);
        u8g2_DrawFrame(u,static_cast<u8g2_uint_t>(tx-1),static_cast<u8g2_uint_t>(ty-1),3,3);
    }
    for(unsigned i=0;i<2;i++) {
        const int cx=i==0?56:71;
        u8g2_DrawVLine(u,static_cast<u8g2_uint_t>(cx),26,29);
        const int y=40-static_cast<int>(std::lround(std::clamp(model->out.current[4+i],-13.0f,13.0f)));
        u8g2_DrawBox(u,static_cast<u8g2_uint_t>(cx-3),static_cast<u8g2_uint_t>(y-1),7,3);
        const int ty=40-static_cast<int>(std::lround(std::clamp(model->out.target[4+i],-13.0f,13.0f)));
        u8g2_DrawPixel(u,static_cast<u8g2_uint_t>(cx-5),static_cast<u8g2_uint_t>(ty));
    }
}
void enterMenu(astra::Menu *next) {
    if(model->menu)model->menu->rememberCameraPos(model->camera.getPositionTrg());
    model->menu=next;next->forePosInit();next->childPosInit(model->camera.getPosition());model->selector.inject(next);
}
void open() { auto *next=model->menu->getNextMenu();if(next&&!next->childMenu.empty())enterMenu(next); }
void back() { if(model->menu->parent)enterMenu(model->menu->parent); }
void resetMenus() {
    model->home.selectIndex=0;model->home.cameraPosMemoryFlag=false;
    for(auto &node:model->nodes){node.selectIndex=0;node.initFlag=false;node.cameraPosMemoryFlag=false;node.cameraPosMemory={0,0};node.boundary={0,3};}
    model->camera.goDirect(0,0);model->selector.x=0;model->selector.y=0;model->selector.w=0;model->selector.h=0;
    model->menu=nullptr;enterMenu(&model->home);model->action=-1;model->popup=false;model->popup_closing=false;
}
void menuAction(unsigned step) {
    if(step==0)return;
    if(step==1||step==2)model->selector.goNext();
    else if(step==3)model->selector.goPreview();
    else if(step==4||step==6||step==17)open();
    else if(step==5)model->selector.go(1);
    else if(step>=7&&step<=15)model->selector.goNext();
    else if(step==16||step==23||step==24)back();
    else if(step==18){model->popup=true;model->popup_closing=false;model->popup_y=-30;}
    else if(step==19)model->popup_closing=true;
    else if(step==20){model->popup=false;enterMenu(&model->home);model->selector.go(2);open();model->selector.go(0);open();}
    else if(step==21||step==22)model->selector.goNext();
}
void renderMenu() {
    auto *u=U8G2_PortContext();u8g2_ClearBuffer(u);
    const auto camera=model->camera.getPosition();
    model->menu->render(camera);model->selector.render(camera);
    if(model->popup) {
        astra::Animation::blur();
        HAL::setDrawType(0);HAL::drawBox(8,model->popup_y,112,27);
        HAL::setDrawType(1);HAL::drawFrame(8,model->popup_y,112,27);
        static const std::string message="MEMORY DEMO";
        HAL::drawEnglish(14,model->popup_y+11,message);
        static const std::string note="BACK/REOPEN OK";
        HAL::drawEnglish(14,model->popup_y+22,note);
    }
}
void advanceMenu() {
    model->menu->update();
    model->selector.update();
    model->camera.update(model->menu,&model->selector);
    if(model->popup) {
        astra::Animation::move(&model->popup_y,model->popup_closing?-30.0f:18.0f,SHOWCASE_ASTRA_SPEED);
        if(model->popup_closing&&model->popup_y<=-29)model->popup=false;
    }
    auto &s=model->out;s.menu_page=255;
    if(model->menu==&model->home)s.menu_page=254;
    else for(unsigned i=0;i<model->nodes.size();i++)if(model->menu==&model->nodes[i])s.menu_page=static_cast<uint8_t>(i);
    s.menu_index=model->menu->selectIndex;s.menu_action=static_cast<uint8_t>(model->action);s.popup=model->popup;
    s.camera_x=model->camera.x;s.camera_y=model->camera.y;
    s.selector_x=model->selector.x;s.selector_y=model->selector.y;s.selector_w=model->selector.w;s.selector_h=model->selector.h;
}
void step(uint64_t elapsed_us) {
    auto &s=model->out;const auto cycle=static_cast<uint32_t>(elapsed_us/SHOWCASE_CYCLE_US);
    const uint64_t phase=elapsed_us%SHOWCASE_CYCLE_US;
    const int stage=phase<SHOWCASE_MENU_US?6:static_cast<int>((phase-SHOWCASE_MENU_US)/SHOWCASE_STICK_STAGE_US);
    if(cycle!=model->previous_cycle||stage!=model->previous_stage) {
        if(stage<6)resetChannels(static_cast<unsigned>(stage));else resetMenus();
        model->previous_stage=stage;model->previous_cycle=cycle;
    }
    s.cycle=cycle;s.section=stage<6?SHOWCASE_STICKS:SHOWCASE_ASTRA;
    s.section_ms=static_cast<uint32_t>((stage<6?phase-SHOWCASE_MENU_US:phase)/1000U);
    model->dirty=true;
    if(stage<6) {
        const uint64_t local=(phase-SHOWCASE_MENU_US)%SHOWCASE_STICK_STAGE_US;sampleChannels(local);
        unsigned g=0;while(g+1<sizeof(gestures)/sizeof(gestures[0])&&local>=gestures[g+1].us)g++;
        if(g!=s.gesture)retarget(g,local);
    } else {
        const auto target=std::min<unsigned>(static_cast<unsigned>(phase/SHOWCASE_MENU_ACTION_US),SHOWCASE_MENU_ACTIONS-1);
        while(model->action<static_cast<int>(target))menuAction(static_cast<unsigned>(++model->action));
        advanceMenu();
    }
}
}
bool Showcase_Init(uint64_t now_us) {
    static std::array<uint8_t,1024> initial_frame;
    return Showcase_InitWithBuffer(now_us,initial_frame.data());
}
bool Showcase_InitWithBuffer(uint64_t now_us,uint8_t *bootstrap) {
    if(model)return true;
    if(!AstraPort_Init(bootstrap))return false;
    auto &cfg=astra::getUIConfig();
    cfg.tileAnimationSpeed=cfg.listAnimationSpeed=cfg.selectorXAnimationSpeed=cfg.selectorYAnimationSpeed=
        cfg.selectorWidthAnimationSpeed=cfg.selectorHeightAnimationSpeed=cfg.cameraAnimationSpeed=
        cfg.windowAnimationSpeed=cfg.sideBarAnimationSpeed=cfg.logoAnimationSpeed=cfg.popSpeed=SHOWCASE_ASTRA_SPEED;
    model=new(storage)Model;model->epoch=now_us;model->last_us=now_us;
    step(0);return true;
}
bool Showcase_Update(uint64_t now_us) {
    if(!model||now_us<model->last_us)return false;
    model->last_us=now_us;const uint64_t elapsed=now_us-model->epoch;
    const uint64_t target=elapsed/1000000U*75U+(elapsed%1000000U)*75U/1000000U;
    if(target-model->last_tick>8){const uint64_t skip=target-model->last_tick-8;
        model->out.skipped_ticks+=static_cast<uint32_t>(std::min<uint64_t>(skip,UINT32_MAX-model->out.skipped_ticks));model->last_tick+=skip;}
    while(model->last_tick<target){step(tickTime(++model->last_tick));++model->out.updates;}
    return true;
}
bool Showcase_Draw(uint8_t *frame) {
    if(!model||!frame)return false;
    if(model->dirty) {
        bind();
        if(model->out.section==SHOWCASE_ASTRA)renderMenu();else drawSticks();
        ++model->out.renders;model->dirty=false;
    }
    std::memcpy(frame,model->frame.data(),model->frame.size());return true;
}
bool Showcase_GetSnapshot(ShowcaseSnapshot *out) { if(!model||!out)return false;*out=model->out;return true; }
const char *Showcase_AlgorithmName(unsigned algorithm) {
    static const char *names[]={"CRITICAL","BOUNCE","OVERDAMP","OUT QUAD","OUT EXPO","INOUT CUBIC"};
    return algorithm<SHOWCASE_ALGORITHMS?names[algorithm]:"UNKNOWN";
}
