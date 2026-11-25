#include "ecovision_algorithm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static EvInput in;
static EvState s;
static EvOutput o;
static void reset(void) { memset(&in,0,sizeof in); ev_init(&s); }
static void sample(unsigned t, int count, int temp) {
    unsigned i;
    in.now_ms=t;
    for(i=0;i<EV_CHANNELS;i++) {
        in.sample[i].present=1; in.sample[i].valid=1;
        in.sample[i].at_ms=t; in.sample[i].source=1;
        in.sample[i].simulated=1;
    }
    in.sample[EV_TEMP].value=temp; in.sample[EV_RH].value=5000;
    in.sample[EV_LIGHT].value=15000; in.sample[EV_VISION].value=count;
    ev_step(&s,&in,&o); in.fan_actual=o.fan; in.led_actual=o.led;
}
static void tick(unsigned t) {
    unsigned i; in.now_ms=t;
    for(i=0;i<EV_CHANNELS;i++) in.sample[i].present=0;
    ev_step(&s,&in,&o); in.fan_actual=o.fan; in.led_actual=o.led;
}
int main(void) {
    EvDebounce b={0}; unsigned t;
    reset(); sample(0,1,2900); assert(o.occupancy==EV_UNKNOWN && !o.fan);
    sample(1000,1,2900); sample(2000,1,2900);
    assert(o.occupancy==EV_OCCUPIED && o.led && !o.fan);
    for(t=3000;t<=7000;t+=1000) sample(t,1,2900);
    assert(o.fan && o.apply_mask==3);
    for(t=8000;t<=18000;t+=1000) sample(t,1,(t%2000)?2790:2810);
    assert(o.fan); /* hysteresis */
    for(t=19000;t<=24000;t+=1000) sample(t,1,2700);
    assert(!o.fan);
    for(t=25000;t<=55000;t+=1000) sample(t,0,2900);
    assert(o.occupancy==EV_VACANT && !o.fan && !o.led);
    puts("PASS occupancy/dwell/hysteresis/vacancy");

    reset(); sample(0,1,2500); sample(1000,1,2500); sample(2000,1,8000);
    assert(s.channel[EV_TEMP].filtered==2500 && (s.channel[EV_TEMP].reason&EV_JUMP));
    assert(s.channel[EV_TEMP].raw==8000);
    sample(3000,1,9000); assert(!s.channel[EV_TEMP].valid);
    assert(s.channel[EV_TEMP].last_attempt.value==9000);
    assert(s.channel[EV_TEMP].at_ms==2000);
    sample(4000,1,2600); assert(s.channel[EV_TEMP].valid && !s.channel[EV_TEMP].stable);
    assert(s.channel[EV_TEMP].filtered==2600);
    tick(14000); assert(!s.channel[EV_TEMP].valid && s.channel[EV_TEMP].at_ms==4000);
    puts("PASS raw faults/filter/recovery/stale");

    reset(); sample(1000,1,2900); sample(2000,1,2900); sample(3000,1,2900);
    sample(3000,1,2900); assert(s.positive==3); /* duplicate not a frame */
    sample(2000,1,2900); assert(s.channel[EV_TEMP].at_ms==3000);
    tick(8000); assert(o.occupancy==EV_UNKNOWN);
    sample(9000,1,2900); assert(o.occupancy==EV_UNKNOWN);
    sample(10000,1,2900); sample(11000,1,2900); assert(o.occupancy==EV_OCCUPIED);
    in.sample[EV_VISION].valid=0; in.sample[EV_VISION].present=1;
    in.sample[EV_VISION].at_ms=12000; in.now_ms=12000; ev_step(&s,&in,&o);
    assert(o.occupancy==EV_UNKNOWN);
    puts("PASS duplicate/backward/timeout/reacquisition/invalid");

    reset(); for(t=0;t<=7000;t+=1000) sample(t,1,2900);
    assert(o.fan && o.led); tick(12000); assert(o.fan && o.led);
    tick(71999); assert(o.fan); tick(72000); assert(!o.fan && !o.led);
    in.mode=EV_REMOTE; in.fan_actual=1; in.led_actual=1; tick(73000);
    assert(o.apply_mask==0 && o.fan && o.led);
    in.mode=EV_LOCAL_OFF; tick(74000); assert(o.apply_mask==3 && !o.fan && !o.led);
    in.mode=EV_AUTO; tick(75000); assert(!o.fan && !o.led);
    puts("PASS fault hold/off/manual yield/local off");

    reset(); sample(0xFFFFF000u,1,2900); sample(0xFFFFF3E8u,1,2900);
    sample(0xFFFFF7D0u,1,2900); sample(0xFFFFFBB8u,1,2900);
    sample(0xFFFFFFA0u,1,2900); sample(4000,1,2900);
    assert(o.occupancy==EV_OCCUPIED && o.fan);
    assert(ev_debounce(&b,0,1,30)==-1);
    assert(ev_debounce(&b,29,1,30)==-1);
    assert(ev_debounce(&b,30,1,30)==1);
    assert(ev_debounce(&b,31,0,30)==1);
    assert(ev_debounce(&b,32,1,30)==1);
    puts("PASS tick wrap/debounce");
    reset(); sample(100,1,2900); sample(1100,1,2900); sample(2100,1,2900);
    sample(12100,1,2900); assert(!o.fan && o.occupancy==EV_UNKNOWN);
    sample(13100,1,2900); sample(14100,1,2900); assert(!o.fan);
    sample(19100,1,2900); assert(!o.fan); /* vision gap exactly 5 s */
    reset(); sample(100,1,2900); sample(1100,1,2900); sample(2100,1,2900);
    in.now_ms=2200; in.sample[EV_TEMP].present=1;
    in.sample[EV_TEMP].at_ms=3000; ev_step(&s,&in,&o);
    assert(s.channel[EV_TEMP].at_ms==2100 && (s.channel[EV_TEMP].reason&EV_TIME));
    puts("PASS gap cannot bridge dwell/future timestamp");
    reset(); sample(0,0,-4000); assert(s.channel[EV_TEMP].valid);
    sample(1000,0,8500); assert(s.channel[EV_TEMP].valid);
    sample(2000,0,-4001); assert(!s.channel[EV_TEMP].valid && o.thermal==EV_NEED_UNKNOWN);
    assert(o.affected_mask==3 && o.light_need==EV_NEED_UNKNOWN);
    in.now_ms=3000;
    in.sample[EV_RH].present=1;in.sample[EV_RH].at_ms=3000;in.sample[EV_RH].value=10001;
    in.sample[EV_LIGHT].present=1;in.sample[EV_LIGHT].at_ms=3000;in.sample[EV_LIGHT].value=65536;
    in.sample[EV_VISION].present=1;in.sample[EV_VISION].at_ms=3000;in.sample[EV_VISION].value=21;
    ev_step(&s,&in,&o);
    assert(!s.channel[EV_RH].valid && !s.channel[EV_LIGHT].valid && !o.vision_valid);
    puts("PASS all channel ranges/unknown/affected controls");
    reset(); for(t=0;t<=7000;t+=1000) sample(t,1,2900);
    ev_reset_vision(&s);tick(7100);
    assert(o.occupancy==EV_UNKNOWN && !o.vision_valid && o.fan);
    sample(8000,1,2900);sample(9000,1,2900);assert(o.occupancy==EV_UNKNOWN);
    sample(10000,1,2900);assert(o.occupancy==EV_OCCUPIED);
    ev_init(&s);memset(&in,0,sizeof in);tick(0);
    assert(o.occupancy==EV_UNKNOWN && !o.fan && !o.led);
    puts("PASS session reset/reboot");
    printf("RESOURCE state=%u input=%u output=%u debounce=%u bytes\n",
        (unsigned)sizeof s,(unsigned)sizeof in,(unsigned)sizeof o,(unsigned)sizeof b);
    return 0;
}
