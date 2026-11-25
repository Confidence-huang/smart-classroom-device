#include "app_control.h"
#include "app_environment.h"
#include "app_vision.h"
#include "cmsis_os.h"
#include "main.h"
#include <string.h>

static ControlPolicy policy;
static ControlOutput applied;
/* The network task submits; the output task decides and completes after writing both drivers. */
static enum { COMMAND_IDLE, COMMAND_QUEUED, COMMAND_WRITING, COMMAND_DONE } command_state;
static ControlCommand command;
static ControlReply reply;
static char applied_command[65];
static char last_link[17],last_boot[17];
static uint32_t last_seq,last_errors,last_rejected;
static uint32_t last_invalid_frames; /* Last interruption already handled by AUTO. */
static bool have_seq;

void AppControl_Init(void) {
    Control_Init(&policy);applied=policy.output;
    command_state=COMMAND_IDLE;
    applied_command[0]=0;
    memset(last_link,0,sizeof last_link);memset(last_boot,0,sizeof last_boot);
    have_seq=false;last_errors=0;last_rejected=0;last_invalid_frames=0;
}

bool AppControl_Queue(const ControlCommand *new_command) {
    taskENTER_CRITICAL();
    bool available=command_state==COMMAND_IDLE;
    if(available) {
        command=*new_command;
        command_state=COMMAND_QUEUED;
    }
    taskEXIT_CRITICAL();
    return available;
}

bool AppControl_TakeReply(ControlReply *result) {
    taskENTER_CRITICAL();
    bool done=command_state==COMMAND_DONE;
    if(done) {
        *result=reply;
        command_state=COMMAND_IDLE;
    }
    taskEXIT_CRITICAL();
    return done;
}

static void choose_command(uint32_t now) {
    /* Polling B1 before this function ensures a same-cycle button action wins. */
    if(command_state!=COMMAND_QUEUED)return;
    if((int32_t)(now-command.expires_at)>=0)reply.result=CONTROL_EXPIRED;
    else if(policy.output.control_epoch!=command.epoch)reply.result=CONTROL_EPOCH_MISMATCH;
    else if(!Control_Request(&policy,now,command.epoch,command.hold_s,command.fan_level,command.led))
        reply.result=CONTROL_LOCAL_LOCKED;
    else reply.result=CONTROL_APPLIED;
    command_state=COMMAND_WRITING;
}

ControlOutput AppControl_Poll(void) {
    EvVision vision;EnvironmentSnapshot environment;
    AppVision_GetSnapshot(&vision);AppEnvironment_GetSnapshot(&environment);
    uint32_t now=HAL_GetTick();
    bool session=vision.active && (memcmp(last_link,vision.link,17)!=0 || memcmp(last_boot,vision.boot,17)!=0);
    if(session) {
        memcpy(last_link,vision.link,17);memcpy(last_boot,vision.boot,17);
        have_seq=false;last_errors=vision.errors;last_rejected=vision.rejected;
        last_invalid_frames=vision.invalid_frames; /* A new link already resets occupancy confirmation. */
    }
    /* Reset confirmation before counting another frame if any accepted sample was invalid between polls. */
    bool damaged=vision.errors!=last_errors || vision.rejected!=last_rejected ||
                 vision.invalid_frames!=last_invalid_frames;
    last_errors=vision.errors;last_rejected=vision.rejected;last_invalid_frames=vision.invalid_frames;
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
    choose_command(now);
    ControlOutput output=policy.output;
    taskEXIT_CRITICAL();
    return output;
}

void AppControl_PublishApplied(const ControlOutput *output) {
    taskENTER_CRITICAL();
    applied=*output;
    if(command_state==COMMAND_WRITING) {
        /* The caller reaches here only after both driver operations succeeded. */
        if(reply.result==CONTROL_APPLIED)memcpy(applied_command,command.id,sizeof applied_command);
        reply.output=applied;
        command_state=COMMAND_DONE;
    }
    taskEXIT_CRITICAL();
}

void AppControl_GetRemoteSnapshot(ControlOutput *output,char last_command[65]) {
    taskENTER_CRITICAL();
    *output=applied;
    memcpy(last_command,applied_command,sizeof applied_command);
    taskEXIT_CRITICAL();
}

void AppControl_GetSnapshot(ControlOutput *output) {
    taskENTER_CRITICAL();*output=applied;taskEXIT_CRITICAL();
}

void AppControl_SafetyOff(void) {
    taskENTER_CRITICAL();
    if(policy.output.mode!=CONTROL_LOCAL_OFF)Control_LocalOff(&policy);
    taskEXIT_CRITICAL();
}
