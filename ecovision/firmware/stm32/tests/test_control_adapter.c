#include "app_control.h"
#include "app_environment.h"
#include "app_vision.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int critical_depth;
static uint32_t tick;
static bool button_pressed;
static EnvironmentSnapshot environment;
static EvVision vision;
uint32_t HAL_GetTick(void){return tick;}
GPIO_PinState HAL_GPIO_ReadPin(void *port,uint16_t pin) {
    assert(port==GPIOC && pin==GPIO_PIN_13);
    return button_pressed?GPIO_PIN_SET:GPIO_PIN_RESET;
}
void AppEnvironment_GetSnapshot(EnvironmentSnapshot *out){*out=environment;}
void AppVision_GetSnapshot(EvVision *out){*out=vision;}
static ControlOutput frame(uint32_t at,uint32_t seq) {
    tick=at;vision.active=true;vision.valid=true;vision.seq=seq;vision.fresh_at=at;
    environment.sampled_at=at;environment.th_valid=true;environment.temp_x100=2900;
    return AppControl_Poll();
}

/* Model an invalid sample followed by a valid sample before AUTO next reads the snapshot. */
static void invalid_between_polls_resets_confirmation(void) {
    AppControl_Init();
    assert(frame(0,0).occupancy==CONTROL_UNKNOWN);
    assert(frame(1000,1).occupancy==CONTROL_UNKNOWN);
    vision.invalid_frames++;
    ControlOutput out=frame(3000,3);
    assert(out.occupancy==CONTROL_UNKNOWN && !out.led);

    /* The latest valid frame can start a fresh count, but still needs two more frames. */
    assert(AppControl_Poll().occupancy==CONTROL_UNKNOWN);
    assert(frame(4000,4).occupancy==CONTROL_UNKNOWN);
    out=frame(5000,5);
    assert(out.occupancy==CONTROL_OCCUPIED && out.led);
}

/* A matching MQTT request only becomes APPLIED after the driver writes. */
static void remote_mailbox_and_button(void) {
    AppControl_Init();
    button_pressed=false;
    tick=10000;
    ControlCommand request={.epoch=0,.expires_at=11000,.hold_s=30,.fan_level=CONTROL_FAN_MAX,.id="fan-on"};
    ControlReply reply;
    ControlOutput snapshot;
    char last_command[65];
    assert(AppControl_Queue(&request) && !AppControl_Queue(&request));
    ControlOutput out=AppControl_Poll();
    assert(out.mode==CONTROL_REMOTE && out.fan_level==CONTROL_FAN_MAX && out.hold_remaining_s==30);
    assert(!AppControl_TakeReply(&reply));
    AppControl_GetRemoteSnapshot(&snapshot,last_command);
    assert(snapshot.mode==CONTROL_AUTO && !last_command[0]);
    AppControl_PublishApplied(&out);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_APPLIED && reply.output.fan_level==CONTROL_FAN_MAX);
    AppControl_GetRemoteSnapshot(&snapshot,last_command);
    assert(snapshot.fan_level && strcmp(last_command,"fan-on")==0);

    button_pressed=true;
    tick=10100; AppControl_Poll();
    tick=10140; AppControl_Poll();
    tick=12100;
    request.expires_at=13000;
    assert(AppControl_Queue(&request));
    out=AppControl_Poll();
    assert(out.mode==CONTROL_LOCAL_OFF && out.control_epoch==1 && !out.fan_level);
    AppControl_PublishApplied(&out);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_EPOCH_MISMATCH);

    request.epoch=1;request.hold_s=0; /* A cloud AUTO request cannot clear LOCAL_OFF. */
    assert(AppControl_Queue(&request));
    out=AppControl_Poll();
    AppControl_PublishApplied(&out);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_LOCAL_LOCKED);
    request.expires_at=tick;
    assert(AppControl_Queue(&request));
    out=AppControl_Poll();
    AppControl_PublishApplied(&out);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_EXPIRED);
    AppControl_GetRemoteSnapshot(&snapshot,last_command);
    assert(snapshot.mode==CONTROL_LOCAL_OFF && strcmp(last_command,"fan-on")==0);
}

int main(void) {
    AppControl_Init();memcpy(vision.link,"0000000000000001",17);memcpy(vision.boot,"0000000000000002",17);vision.count=1;
    ControlOutput out=frame(0,0);assert(!out.led);
    for(tick=20;tick<1000;tick+=20){out=AppControl_Poll();assert(!out.led);}
    out=frame(1000,1);assert(!out.led);
    out=frame(2000,2);assert(out.led && out.occupancy==CONTROL_OCCUPIED);
    ControlOutput published;AppControl_GetSnapshot(&published);
    assert(!published.led); /* Computed intent is not applied truth yet. */
    AppControl_PublishApplied(&out);AppControl_GetSnapshot(&published);assert(published.led);
    vision.rejected++;tick=2020;out=AppControl_Poll();assert(out.occupancy==CONTROL_UNKNOWN);
    tick=2040;out=AppControl_Poll();assert(out.occupancy==CONTROL_UNKNOWN);
    out=frame(3000,3);assert(out.occupancy==CONTROL_UNKNOWN);
    out=frame(4000,4);assert(out.occupancy==CONTROL_UNKNOWN);
    out=frame(5000,5);assert(out.occupancy==CONTROL_OCCUPIED);
    AppControl_SafetyOff();out=AppControl_Poll();
    assert(out.mode==CONTROL_LOCAL_OFF && !out.led && !out.fan_level && out.control_epoch==1);
    AppControl_PublishApplied(&out);AppControl_GetSnapshot(&published);assert(published.mode==CONTROL_LOCAL_OFF);
    invalid_between_polls_resets_confirmation();
    remote_mailbox_and_button();
    assert(critical_depth==0);
    puts("PASS: adapter frame identity, invalid sample between polls, applied-only snapshot, safety OFF and critical-section balance");
    return 0;
}
