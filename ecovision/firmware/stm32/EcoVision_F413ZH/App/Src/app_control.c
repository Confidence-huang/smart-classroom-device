#include "app_control.h"
#include "app_environment.h"
#include "app_vision.h"
#include "cmsis_os.h"
#include "main.h"
#include <string.h>

static ControlPolicy policy;
static ControlOutput applied;
static char last_link[17],last_boot[17];
static uint32_t last_seq,last_errors,last_rejected;
static bool have_seq;

void AppControl_Init(void) {
    Control_Init(&policy);applied=policy.output;
    memset(last_link,0,sizeof last_link);memset(last_boot,0,sizeof last_boot);
    have_seq=false;last_errors=0;last_rejected=0;
}

ControlOutput AppControl_Poll(void) {
    EvVision vision;EnvironmentSnapshot environment;
    AppVision_GetSnapshot(&vision);AppEnvironment_GetSnapshot(&environment);
    uint32_t now=HAL_GetTick();
    bool session=vision.active && (memcmp(last_link,vision.link,17)!=0 || memcmp(last_boot,vision.boot,17)!=0);
    if(session) {
        memcpy(last_link,vision.link,17);memcpy(last_boot,vision.boot,17);
        have_seq=false;last_errors=vision.errors;last_rejected=vision.rejected;
    }
    bool damaged=vision.errors!=last_errors || vision.rejected!=last_rejected;
    last_errors=vision.errors;last_rejected=vision.rejected;
    ControlInput input={0};
    input.vision_valid=vision.active && vision.valid && !damaged;
    input.vision_age_ms=now-vision.fresh_at;
    input.new_vision_session=session;
    input.new_vision_frame=input.vision_valid && (!have_seq || vision.seq!=last_seq);
    if(input.new_vision_frame){have_seq=true;last_seq=vision.seq;}
    input.count=vision.count;
    input.temperature_valid=environment.th_valid;
    input.temperature_x100=environment.temp_x100;
    input.temperature_age_ms=now-environment.sampled_at;
    /* NUCLEO MB1137 USER B1 is PC13, active high. */
    input.button_pressed=HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)==GPIO_PIN_SET;
    taskENTER_CRITICAL();
    Control_Update(&policy,now,&input);
    ControlOutput output=policy.output;
    taskEXIT_CRITICAL();
    return output;
}

void AppControl_PublishApplied(const ControlOutput *output) {
    taskENTER_CRITICAL();applied=*output;taskEXIT_CRITICAL();
}

void AppControl_GetSnapshot(ControlOutput *output) {
    taskENTER_CRITICAL();*output=applied;taskEXIT_CRITICAL();
}

void AppControl_SafetyOff(void) {
    taskENTER_CRITICAL();
    if(policy.output.mode!=CONTROL_LOCAL_OFF)Control_LocalOff(&policy);
    taskEXIT_CRITICAL();
}
