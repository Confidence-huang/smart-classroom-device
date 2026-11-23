#include "control_policy.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static ControlInput sample(uint8_t count, int32_t temp) {
    ControlInput in={0};
    in.vision_valid=true;in.new_vision_frame=true;in.count=count;
    in.temperature_valid=true;in.temperature_x100=temp;
    return in;
}
static void occupy(ControlPolicy *p, uint32_t now, int32_t temp) {
    ControlInput in=sample(1,temp);
    Control_Update(p,now,&in);Control_Update(p,now+1000U,&in);Control_Update(p,now+2000U,&in);
    assert(p->output.occupancy==CONTROL_OCCUPIED && p->output.led);
}
static void three_real_frames(void) {
    ControlPolicy p;Control_Init(&p);ControlInput in=sample(1,2500);
    assert(!p.output.led && !p.output.fan && p.output.mode==CONTROL_AUTO);
    Control_Update(&p,0,&in);assert(!p.output.led);
    in.new_vision_frame=false;
    for(uint32_t t=10;t<1000;t+=10)Control_Update(&p,t,&in);
    assert(!p.output.led);
    in.new_vision_frame=true;Control_Update(&p,1000,&in);assert(!p.output.led);
    Control_Update(&p,2000,&in);assert(p.output.led);
}
static void absence_requires_thirty_seconds(void) {
    ControlPolicy p;Control_Init(&p);occupy(&p,0,2500);ControlInput in=sample(0,2500);
    for(uint32_t t=3000;t<33000;t+=1000){Control_Update(&p,t,&in);assert(p.output.led);}
    Control_Update(&p,33000,&in);assert(p.output.occupancy==CONTROL_VACANT && !p.output.led && !p.output.fan);
}
static void invalid_resets_confirmation(void) {
    ControlPolicy p;Control_Init(&p);occupy(&p,0,2500);ControlInput in=sample(0,2500);
    for(uint32_t t=3000;t<32000;t+=1000)Control_Update(&p,t,&in);
    in.vision_valid=false;Control_Update(&p,32000,&in);assert(p.output.occupancy==CONTROL_UNKNOWN && p.output.led);
    in=sample(0,2500);
    for(uint32_t t=33000;t<63000;t+=1000){Control_Update(&p,t,&in);assert(p.output.led);}
    Control_Update(&p,63000,&in);assert(!p.output.led);
}
static void hysteresis_and_fault_hold(void) {
    ControlPolicy p;Control_Init(&p);occupy(&p,0,2800);ControlInput in=sample(1,2800);
    for(uint32_t t=3000;t<7000;t+=1000){Control_Update(&p,t,&in);assert(!p.output.fan);}
    Control_Update(&p,7000,&in);assert(p.output.fan);
    in.temperature_x100=2750;Control_Update(&p,8000,&in);assert(p.output.fan);
    in.temperature_x100=2700;
    for(uint32_t t=9000;t<14000;t+=1000){Control_Update(&p,t,&in);assert(p.output.fan);}
    Control_Update(&p,14000,&in);assert(!p.output.fan);
    in.temperature_x100=2800;Control_Update(&p,15000,&in);
    in.temperature_valid=false;Control_Update(&p,18000,&in);assert(p.output.led && !p.output.fan);
    Control_Update(&p,77999,&in);assert(p.output.led);
    Control_Update(&p,78000,&in);assert(!p.output.led && !p.output.fan);
}
static void freshness_and_session(void) {
    ControlPolicy p;Control_Init(&p);occupy(&p,0,2900);ControlInput in=sample(1,2900);
    in.vision_age_ms=5000;Control_Update(&p,7000,&in);assert(p.output.occupancy==CONTROL_UNKNOWN && p.output.led);
    in=sample(1,2900);in.new_vision_session=true;Control_Update(&p,8000,&in);assert(p.output.occupancy==CONTROL_UNKNOWN);
    in.new_vision_session=false;Control_Update(&p,9000,&in);assert(p.output.occupancy==CONTROL_UNKNOWN);
    Control_Update(&p,10000,&in);assert(p.output.occupancy==CONTROL_OCCUPIED);
    in.temperature_age_ms=10000;Control_Update(&p,11000,&in);assert(p.fault_active);
}
static void short_and_long_button(void) {
    ControlPolicy p;Control_Init(&p);occupy(&p,0,2900);ControlInput in=sample(1,2900);
    in.new_vision_frame=false;in.button_pressed=true;Control_Update(&p,3000,&in);
    Control_Update(&p,3029,&in);assert(p.output.control_epoch==0);
    Control_Update(&p,3030,&in);Control_Update(&p,4999,&in);assert(p.output.mode==CONTROL_AUTO);
    Control_Update(&p,5000,&in);assert(p.output.mode==CONTROL_LOCAL_OFF && p.output.control_epoch==1 && !p.output.led && !p.output.fan);
    in.button_pressed=false;Control_Update(&p,6000,&in);Control_Update(&p,6030,&in);
    assert(p.output.mode==CONTROL_LOCAL_OFF && p.output.control_epoch==1);
    in.button_pressed=true;Control_Update(&p,7000,&in);Control_Update(&p,7030,&in);
    in.button_pressed=false;Control_Update(&p,7100,&in);Control_Update(&p,7130,&in);
    assert(p.output.mode==CONTROL_AUTO && p.output.control_epoch==2);
}
static void release_before_threshold_is_short(void) {
    ControlPolicy p;Control_Init(&p);ControlInput in=sample(0,2500);in.button_pressed=true;
    Control_Update(&p,0,&in);Control_Update(&p,30,&in);
    in.button_pressed=false;Control_Update(&p,1999,&in);Control_Update(&p,2000,&in);Control_Update(&p,2029,&in);
    assert(p.output.mode==CONTROL_AUTO && p.output.control_epoch==1);
}
static void bounce_has_no_action(void) {
    ControlPolicy p;Control_Init(&p);ControlInput in=sample(0,2500);in.button_pressed=true;
    Control_Update(&p,0,&in);in.button_pressed=false;Control_Update(&p,10,&in);Control_Update(&p,50,&in);
    assert(p.output.control_epoch==0);
}
static void clock_wrap_and_epoch_exhaustion(void) {
    ControlPolicy p;Control_Init(&p);uint32_t t=UINT32_MAX-4000U;occupy(&p,t,2800);ControlInput in=sample(1,2800);
    for(uint32_t step=3000;step<=7000;step+=1000)Control_Update(&p,t+step,&in);
    assert(p.output.fan);
    p.output.control_epoch=UINT32_MAX;Control_LocalOff(&p);
    assert(p.output.requires_new_boot && p.output.control_epoch==UINT32_MAX && !p.output.fan && !p.output.led);
}
int main(void) {
    three_real_frames();absence_requires_thirty_seconds();invalid_resets_confirmation();
    hysteresis_and_fault_hold();freshness_and_session();short_and_long_button();
    release_before_threshold_is_short();bounce_has_no_action();clock_wrap_and_epoch_exhaustion();
    puts("PASS: AUTO occupancy, duplicates, 30s absence, invalid/session/freshness, hysteresis, 60s fault, button boundaries, wrap and epoch exhaustion");
    return 0;
}
