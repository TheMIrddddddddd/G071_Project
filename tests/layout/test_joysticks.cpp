#include "app_joystick.h"
#include "joystick_follow.h"
#include "u8g2_port.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <new>

static unsigned allocations;
void *operator new(std::size_t n) {
  ++allocations;
  if(void *p=std::malloc(n?n:1))return p;
  throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p,std::size_t) noexcept { std::free(p); }
void operator delete[](void *p,std::size_t) noexcept { std::free(p); }

static uint8_t frame[1024];
static const uint64_t epoch=(UINT64_C(1)<<48)+123;
static JoystickSnapshot snapshot() {
  JoystickSnapshot s{};assert(Joystick_GetSnapshot(&s));return s;
}
static bool pixel(unsigned x,unsigned y) { return frame[y/8*128+x]&(1U<<(y%8)); }
static void render() {
  auto *u=U8G2_PortContext();u8g2_ClearBuffer(u);assert(app_joystick_draw(u));
  for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x) {
    const bool circle=y>=24&&y<=58&&((x>=6&&x<=40)||(x>=87&&x<=121));
    const bool single=y>=25&&y<=57&&((x>=53&&x<=61)||(x>=66&&x<=74));
    if(!circle&&!single)assert(!pixel(x,y));
  }
  for(unsigned cx:{23U,104U}) {
    assert(pixel(cx-17,41)&&pixel(cx+17,41)&&pixel(cx,24)&&pixel(cx,58));
    for(unsigned d=14;d<=16;++d)
      assert(pixel(cx-d,41)&&pixel(cx+d,41)&&pixel(cx,41-d)&&pixel(cx,41+d));
  }
  for(unsigned cx:{57U,70U})for(int dx=-4;dx<=4;++dx)assert(pixel(cx+dx,41));
}
static void near(float actual,float expected,float tolerance=.0002f) {
  assert(std::isfinite(actual)&&std::abs(actual-expected)<tolerance);
}
static float critical(float target,float seconds) {
  return target*(1.0f-(1.0f+10.0f*seconds)*std::exp(-10.0f*seconds));
}
int main(int argc,char **argv) {
  assert(argc==2);
  assert(!app_joystick_update(0));
  const app_joystick_targets_t zero{};
  assert(!app_joystick_set_targets(0,&zero));
  assert(!app_joystick_draw(nullptr));
  U8G2_PortInit(frame);
  const unsigned before=allocations;
  assert(app_joystick_init(epoch));
  const char *which=argv[1];
  if(!std::strcmp(which,"geometry")) {
    render();
    for(unsigned cx:{23U,104U})assert(pixel(cx,41));
    assert(app_joystick_set_targets(epoch,&zero));
    assert(app_joystick_update(epoch+400000));render();
  } else if(!std::strcmp(which,"critical")||!std::strcmp(which,"repeat")) {
    const app_joystick_targets_t target{1,0,0,1,.5f,-.5f};
    assert(app_joystick_set_targets(epoch,&target));
    float last=0;
    for(uint64_t us=40000;us<=3000000;us+=40000) {
      assert(app_joystick_update(epoch+us));
      if(!std::strcmp(which,"repeat"))assert(app_joystick_set_targets(epoch+us,&target));
      const auto s=snapshot();const float t=static_cast<float>(us)*1e-6f;
      near(s.left_current.x,critical(10,t),.021f);
      near(s.right_current.y,-critical(10,t),.021f);
      near(s.single_current.x,critical(5.5f,t),.021f);
      near(s.single_current.y,-critical(5.5f,t),.021f);
      assert(s.left_current.x>=last&&s.left_current.x<=10);last=s.left_current.x;
    }
    const auto s=snapshot();assert(s.settled);near(s.left_current.x,10);render();
  } else if(!std::strcmp(which,"retarget")) {
    app_joystick_targets_t target{1,0,0,0,0,0};
    assert(app_joystick_set_targets(epoch,&target));
    assert(app_joystick_update(epoch+200000));
    const float x=snapshot().left_current.x;
    const float velocity=10*100*.2f*std::exp(-2.0f);
    target.left_x=-1;assert(app_joystick_set_targets(epoch+200000,&target));
    near(snapshot().left_current.x,x); // no position jump on target reversal
    assert(app_joystick_update(epoch+240000));
    const float expected=-10+((x+10)+(velocity+10*(x+10))*.04f)*std::exp(-.4f);
    near(snapshot().left_current.x,expected);
    assert(app_joystick_update(epoch+4000000));near(snapshot().left_current.x,-10);render();
  } else if(!std::strcmp(which,"cadence")) {
    const app_joystick_targets_t target{1,0,0,0,0,0};
    assert(app_joystick_set_targets(epoch,&target));
    for(unsigned us=1000;us<40000;us+=1000) {
      assert(app_joystick_update(epoch+us));assert(snapshot().sample_us==epoch);
    }
    assert(app_joystick_update(epoch+40000));near(snapshot().left_current.x,critical(10,.04f));
    assert(app_joystick_update(epoch+40000));assert(snapshot().sample_us==epoch+40000);
  } else if(!std::strcmp(which,"guards")) {
    app_joystick_targets_t target{1,0,0,0,0,0};
    assert(app_joystick_set_targets(epoch,&target));
    assert(app_joystick_update(epoch+200000));const auto old=snapshot();
    target.single_right=std::numeric_limits<float>::quiet_NaN();
    assert(!app_joystick_set_targets(epoch+240000,&target));
    target.single_right=std::numeric_limits<float>::infinity();
    assert(!app_joystick_set_targets(epoch+240000,&target));
    assert(!app_joystick_set_targets(epoch+240000,nullptr));
    assert(!app_joystick_update(epoch+199999));
    assert(!app_joystick_set_targets(epoch,&zero));
    const auto s=snapshot();assert(s.sample_us==old.sample_us);
    near(s.left_current.x,old.left_current.x);near(s.left_target.x,old.left_target.x);
  } else if(!std::strcmp(which,"bounds")) {
    for(unsigned step=0;step<180;++step) {
      const float sign=step%2?-1.0f:1.0f;
      const app_joystick_targets_t target{sign,sign,-sign,-sign,sign*1e30f,-sign*1e30f};
      const uint64_t now=epoch+step*200000U;
      assert(app_joystick_set_targets(now,&target));
      for(unsigned tick=0;tick<200000;tick+=40000) {
        assert(app_joystick_update(now+tick));const auto s=snapshot();
        near(s.left_target.x*s.left_target.x+s.left_target.y*s.left_target.y,100,.001f);
        assert(std::abs(s.single_target.x)==11&&std::abs(s.single_target.y)==11);
        render();
      }
    }
  } else if(!std::strcmp(which,"demo")) {
    float minima[6]={},maxima[6]={};
    for(uint64_t us=0;us<24000000;us+=40000) {
      assert(app_joystick_update(epoch+us));const auto s=snapshot();
      const float values[]={s.left_current.x,s.left_current.y,s.right_current.x,
                            s.right_current.y,s.single_current.x,s.single_current.y};
      for(unsigned i=0;i<6;++i){minima[i]=std::min(minima[i],values[i]);maxima[i]=std::max(maxima[i],values[i]);}
      if(us==1000000){assert(s.left_current.x>0);near(s.right_current.y,0);}
      render();
    }
    for(unsigned i=0;i<6;++i)assert(minima[i]<-5&&maxima[i]>5);
  } else if(!std::strcmp(which,"idle")) {
    assert(app_joystick_set_targets(epoch,&zero));render();uint8_t previous[1024];
    std::memcpy(previous,frame,sizeof(frame));
    for(unsigned i=1;i<=200;++i) {
      assert(app_joystick_update(epoch+i*40000U));render();
      assert(!std::memcmp(previous,frame,sizeof(frame))&&snapshot().settled);
    }
  } else if(!std::strcmp(which,"long_time")) {
    const app_joystick_targets_t target{1,0,0,0,1,-1};
    assert(app_joystick_set_targets(epoch,&target));
    assert(app_joystick_update(epoch+120000));near(snapshot().left_current.x,critical(10,.12f));
    assert(app_joystick_update(epoch+UINT64_C(21600000000)));assert(snapshot().settled);
    near(snapshot().left_current.x,10);render();
  } else assert(false);
  assert(allocations==before); // initialization, retargeting, sampling and drawing allocate nothing
  std::printf("PASS: joystick %s; no C++ allocations\n",which);
}
