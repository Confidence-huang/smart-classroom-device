#include "app_control.h"
#include "app_ac_demo.h"
#include "app_environment.h"
#include "app_vision.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int critical_depth;
static uint32_t tick;
static bool button_pressed;
static unsigned ac_requests;
static uint8_t ac_temperature;
static AcModuleResult ac_module_result;
bool AppAcDemo_Ready(void) { return true; }
void AppAcDemo_Poll(uint32_t now) { (void)now; assert(critical_depth==0); }
void AppLog_Printf(const char *format, ...) { (void)format; }
AcModuleResult AppAcDemo_Send(uint8_t temperature_c) {
    assert(critical_depth==0);
    ++ac_requests;
    ac_temperature=temperature_c;
    return ac_module_result;
}
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

static void demo_is_independent_and_obeys_local_lock(void) {
    AppControl_Init();
    button_pressed=false;
    tick=20000;
    ControlCommand request={.epoch=0,.expires_at=21000,.ac_demo=true,.ac_temp_c=23,.id="demo-on"};
    ControlReply reply;
    assert(AppControl_Queue(&request));
    ControlOutput output=AppControl_Poll();
    assert(output.mode==CONTROL_AUTO && !output.fan_level && !output.led && !ac_requests);
    AppControl_PublishApplied(&output);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_AC_ACCEPTED);
    assert(ac_requests==1 && ac_temperature==23);

    AppControl_SafetyOff();
    request.epoch=1;
    request.ac_temp_c=0;
    assert(AppControl_Queue(&request));
    output=AppControl_Poll();
    AppControl_PublishApplied(&output);
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_LOCAL_LOCKED && ac_requests==1);
}

/* Feed a fresh sample each second; use the same poll and publish order as the output task. */
static ControlOutput ac_sample(uint32_t at,uint8_t people,int32_t temp,int32_t humidity,bool valid) {
    tick=at;
    vision.active=true;
    vision.valid=valid;
    vision.count=people;
    vision.fresh_at=at;
    ++vision.seq;
    environment.th_valid=true;
    environment.sampled_at=at;
    environment.temp_x100=temp;
    environment.humi_x100=humidity;
    ControlOutput out=AppControl_Poll();
    AppControl_PublishApplied(&out);
    return out;
}

static void ac_scenario_start(void) {
    AppControl_Init();
    button_pressed=false;
    ac_requests=0;
    ac_module_result=AC_MODULE_ACK;
}

static void automatic_vacancy_and_thermostat(void) {
    ac_scenario_start();
    /* Occupancy is confirmed at 2 s, so the first hot interval matures at 302 s. */
    for(uint32_t at=0;at<302000U;at+=1000U)ac_sample(at,1,2900,5000,true);
    assert(ac_requests==0);
    ac_sample(302000U,1,2900,5000,true);
    assert(ac_requests==1 && ac_temperature==26);
    for(uint32_t at=303000U;at<603000U;at+=1000U)ac_sample(at,1,2700,5000,true);
    assert(ac_requests==1);
    ac_sample(603000U,1,2700,5000,true);
    assert(ac_requests==2 && ac_temperature==0);

    /* The first zero starts the existing vacancy timer. One OFF synchronizes the OLED. */
    for(uint32_t at=604000U;at<634000U;at+=1000U)ac_sample(at,0,2900,5000,true);
    assert(ac_requests==2);
    ControlOutput out=ac_sample(634000U,0,2900,5000,true);
    assert(out.occupancy==CONTROL_VACANT && !out.led && !out.fan_level);
    assert(ac_requests==3 && ac_temperature==0);
    ac_sample(635000U,0,2900,5000,true);
    assert(ac_requests==3);

    ControlCommand manual={.epoch=out.control_epoch,.expires_at=637000U,.ac_demo=true,.ac_temp_c=23};
    assert(AppControl_Queue(&manual));
    ac_sample(636000U,0,2900,5000,true);
    ControlReply reply;
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_LOCAL_LOCKED && ac_requests==3);
}

static void humidity_has_its_own_confirmation_and_manual_off_holds(void) {
    ac_scenario_start();
    /* Adjacent heat and humidity intervals cannot add up to five minutes. */
    for(uint32_t at=0;at<152000U;at+=1000U)ac_sample(at,1,2900,5000,true);
    for(uint32_t at=152000U;at<452000U;at+=1000U)ac_sample(at,1,2800,7000,true);
    assert(ac_requests==0);
    ac_sample(452000U,1,2800,7000,true);
    assert(ac_requests==1 && ac_temperature==26);

    ControlCommand request={.epoch=0,.expires_at=454000U,.ac_demo=true,.ac_temp_c=0};
    assert(AppControl_Queue(&request));
    ac_sample(453000U,1,2800,7000,true);
    ControlReply reply;
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_AC_ACCEPTED);
    assert(ac_requests==2 && ac_temperature==0);
    for(uint32_t at=454000U;at<873000U;at+=1000U)ac_sample(at,1,2800,7000,true);
    assert(ac_requests==2);
    ac_sample(873000U,1,2800,7000,true);
    assert(ac_requests==3 && ac_temperature==26);
}

static void sensor_fault_stops_the_demo_once_and_local_lock_suppresses_off(void) {
    ac_scenario_start();
    ControlCommand request={.epoch=0,.expires_at=1000U,.ac_demo=true,.ac_temp_c=23};
    ac_module_result=AC_MODULE_UNKNOWN;
    assert(AppControl_Queue(&request));
    ac_sample(0,1,2900,5000,false);
    ControlReply reply;
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_AC_UNKNOWN);
    /* UNKNOWN does not mean VACANT. After one minute, send a separate fault OFF. */
    ac_sample(59000U,1,2900,5000,false);
    assert(ac_requests==1 && ac_temperature==23);
    ac_sample(60000U,1,2900,5000,false);
    ac_sample(61000U,1,2900,5000,false);
    assert(ac_requests==2 && ac_temperature==0);

    AppControl_SafetyOff();
    for(uint32_t at=62000U;at<=93000U;at+=1000U)ac_sample(at,0,2900,5000,true);
    assert(ac_requests==2);
}

static void vacancy_overrides_web_temperature_and_new_occupancy_rearms_auto(void) {
    ac_scenario_start();
    for(uint32_t at=0;at<2000U;at+=1000U)ac_sample(at,1,2900,5000,true);
    ControlCommand request={.epoch=0,.expires_at=3000U,.ac_demo=true,.ac_temp_c=23};
    assert(AppControl_Queue(&request));
    ac_sample(2000U,1,2900,5000,true);
    ControlReply reply;
    assert(AppControl_TakeReply(&reply) && reply.result==CONTROL_AC_ACCEPTED);
    assert(ac_requests==1 && ac_temperature==23);

    /* A single zero interrupts the thermal timer without making the room vacant. */
    ac_sample(3000U,0,2900,5000,true);
    ac_sample(4000U,1,2900,5000,true);
    for(uint32_t at=5000U;at<35000U;at+=1000U)ac_sample(at,0,2900,5000,true);
    assert(ac_requests==1);
    ac_sample(35000U,0,2900,5000,true);
    assert(ac_requests==2 && ac_temperature==0);
    for(uint32_t at=36000U;at<338000U;at+=1000U)ac_sample(at,1,2900,5000,true);
    assert(ac_requests==2);
    ac_sample(338000U,1,2900,5000,true);
    assert(ac_requests==3 && ac_temperature==26);
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
    demo_is_independent_and_obeys_local_lock();
    automatic_vacancy_and_thermostat();
    humidity_has_its_own_confirmation_and_manual_off_holds();
    sensor_fault_stops_the_demo_once_and_local_lock_suppresses_off();
    vacancy_overrides_web_temperature_and_new_occupancy_rearms_auto();
    assert(critical_depth==0);
    puts("PASS: adapter frame identity, invalid sample between polls, applied-only snapshot, safety OFF and critical-section balance");
    puts("PASS: demo AC heat, humidity, cooldown, vacancy, manual hold, uncertain ACK, sensor fault and local lock");
    return 0;
}
