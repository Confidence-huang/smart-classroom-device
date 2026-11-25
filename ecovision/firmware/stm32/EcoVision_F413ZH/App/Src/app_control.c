#include "app_control.h"
#include "app_ac_demo.h"
#include "app_environment.h"
#include "app_log.h"
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

/* Separate demo AC state owned by the output task. UART replies cannot report F103's state. */
enum {
    AC_SETPOINT_C = 26, AC_HOT_X100 = 2900, AC_WARM_X100 = 2800,
    AC_COOL_X100 = 2700, AC_HUMID_X100 = 7000, AC_CONFIRM_MS = 300000,
    AC_SENSOR_FAULT_MS = 60000, AC_MANUAL_HOLD_MS = 120000
};
typedef struct {
    uint32_t since;
    bool active, attempted; /* At most one transmission per continuous qualifying interval. */
} AcInterval;
static struct {
    AcInterval hot, humid, cool, fault;
    uint32_t manual_at, epoch;
    bool manual, may_be_on, vacancy_attempted;
    bool fresh_temperature, fresh_positive;
    int32_t temperature_x100, humidity_x100;
} ac;

void AppControl_Init(void) {
    Control_Init(&policy);applied=policy.output;
    command_state=COMMAND_IDLE;
    applied_command[0]=0;
    memset(last_link,0,sizeof last_link);memset(last_boot,0,sizeof last_boot);
    have_seq=false;last_errors=0;last_rejected=0;last_invalid_frames=0;
    memset(&ac,0,sizeof ac);
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
    else if(command.ac_demo) {
        /* Vacancy can stop the demo even during manual control. ON waits for people to return. */
        bool locked=policy.output.mode==CONTROL_LOCAL_OFF ||
                    (command.ac_temp_c!=0U && policy.output.occupancy==CONTROL_VACANT);
        reply.result=locked?CONTROL_LOCAL_LOCKED:CONTROL_AC_ACCEPTED;
    }
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

    ac.fresh_temperature=Control_TemperatureFresh(&input);
    ac.fresh_positive=input.vision_valid && input.count>0U && input.count<=20U;
    ac.temperature_x100=environment.temp_x100;
    ac.humidity_x100=environment.humi_x100;
    return output;
}

/* Start and reset each temperature/humidity interval independently. */
static bool ac_confirm(AcInterval *interval,bool qualifies,uint32_t now,uint32_t duration) {
    if(!qualifies) {
        *interval=(AcInterval){0};
        return false;
    }
    if(!interval->active) *interval=(AcInterval){.since=now,.active=true};
    return !interval->attempted && now-interval->since>=duration;
}

static AcModuleResult ac_send(uint8_t temperature,const char *trigger) {
    AcModuleResult result=AppAcDemo_Send(temperature);
    /* A lost ACK may mean ON was sent. Remember that possibility until OFF is acknowledged. */
    if(result==AC_MODULE_ACK)ac.may_be_on=temperature!=0U;
    if(result==AC_MODULE_UNKNOWN && temperature!=0U)ac.may_be_on=true;
    const char *status=result==AC_MODULE_ACK?"MODULE_ACK":
                       result==AC_MODULE_REJECTED?"MODULE_REJECTED":"MODULE_UNKNOWN";
    AppLog_Printf("[AC] %s set=%u %s\r\n",trigger,(unsigned)temperature,status);
    return result;
}

static void ac_automatic(const ControlOutput *output,uint32_t now,bool manual_sent) {
    bool vacant=output->occupancy==CONTROL_VACANT;
    bool unlocked=output->mode!=CONTROL_LOCAL_OFF && !output->requires_new_boot;
    /* A local epoch change or explicit restore AUTO ends the independent demo AC hold. */
    if(ac.epoch!=output->control_epoch ||
       (command_state==COMMAND_WRITING && reply.result==CONTROL_APPLIED && command.hold_s==0U))
        ac.manual=false;
    ac.epoch=output->control_epoch;
    if(ac.manual && now-ac.manual_at>=AC_MANUAL_HOLD_MS)ac.manual=false;
    if(!vacant)ac.vacancy_attempted=false;

    bool thermal=unlocked && output->mode==CONTROL_AUTO && !ac.manual && !manual_sent &&
                 output->occupancy==CONTROL_OCCUPIED && ac.fresh_positive && ac.fresh_temperature;
    bool hot=ac_confirm(&ac.hot,thermal && ac.temperature_x100>=AC_HOT_X100,now,AC_CONFIRM_MS);
    bool humid=ac_confirm(&ac.humid,thermal && ac.temperature_x100>=AC_WARM_X100 &&
                         ac.humidity_x100>=AC_HUMID_X100 && ac.humidity_x100<=10000,now,AC_CONFIRM_MS);
    bool cool=ac_confirm(&ac.cool,thermal && ac.temperature_x100<=AC_COOL_X100,now,AC_CONFIRM_MS);
    bool fault=ac_confirm(&ac.fault,unlocked && !vacant &&
                         (output->occupancy==CONTROL_UNKNOWN || !ac.fresh_temperature),now,AC_SENSOR_FAULT_MS);

    if(!unlocked || manual_sent || !AppAcDemo_Ready())return;
    if(vacant && !ac.vacancy_attempted) {
        ac.vacancy_attempted=true;
        ac.manual=false;
        (void)ac_send(0U,"AUTO_VACANT");
    } else if(ac.may_be_on && (fault || cool)) {
        if(fault)ac.fault.attempted=true;
        else ac.cool.attempted=true;
        (void)ac_send(0U,fault?"AUTO_SENSOR_FAULT":"AUTO_COOL");
    } else if(!ac.may_be_on && (hot || humid)) {
        /* One ON attempt covers both overlapping heat triggers. */
        ac.hot.attempted=true;
        ac.humid.attempted=true;
        (void)ac_send(AC_SETPOINT_C,hot?"AUTO_HOT":"AUTO_HUMID");
    }
}

void AppControl_PublishApplied(const ControlOutput *output) {
    /* Poll has already handled B1. Keep the bounded UART exchange outside the critical section. */
    uint32_t now=HAL_GetTick();
    bool manual_sent=false;
    if(command_state==COMMAND_WRITING && reply.result==CONTROL_AC_ACCEPTED) {
        if((int32_t)(now-command.expires_at)>=0)reply.result=CONTROL_EXPIRED;
        else {
            AcModuleResult result=ac_send(command.ac_temp_c,"MANUAL");
            manual_sent=true;
            if(result==AC_MODULE_REJECTED)reply.result=CONTROL_AC_REJECTED;
            if(result==AC_MODULE_UNKNOWN)reply.result=CONTROL_AC_UNKNOWN;
            if(result!=AC_MODULE_REJECTED) {
                ac.manual=true;
                ac.manual_at=now;
                if(output->occupancy==CONTROL_VACANT)ac.vacancy_attempted=true;
            }
        }
    }
    AppAcDemo_Poll(now);
    ac_automatic(output,now,manual_sent);
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
