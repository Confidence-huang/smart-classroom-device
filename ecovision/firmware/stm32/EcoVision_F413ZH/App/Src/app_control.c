#include "app_control.h"
#include "app_ac_demo.h"
#include "app_environment.h"
#include "app_log.h"
#include "app_vision.h"
#include "cmsis_os.h"
#include "main.h"
#include <string.h>
#ifdef ECOVISION_DAYLIGHT_SHADOW
#include "app_log.h"
#include "ecovision_core.h"

static EvCoreState daylight_observer;
static uint8_t virtual_lamp;
static struct {
    uint32_t now;
    int32_t lux;
    uint8_t valid, stable, occupancy, advice, virtual_state;
} daylight_report;

static void observe_daylight(const EnvironmentSnapshot *env,const ControlOutput *decision,
                             uint32_t now,bool force_off) {
    EvSample lux={.at_ms=env->sampled_at,.value=(int32_t)env->lux_x100,
                  .present=!daylight_observer.lux.has_seen||
                           daylight_observer.lux.seen_ms!=env->sampled_at,
                  .valid=env->lux_valid,.source=3};
    const uint8_t occupancy=decision->occupancy==CONTROL_OCCUPIED?EV_OCCUPIED:
        decision->occupancy==CONTROL_VACANT?EV_VACANT:EV_UNKNOWN;
    const uint8_t mode=decision->mode==CONTROL_LOCAL_OFF?EV_LOCAL_OFF:EV_AUTO;
    const uint8_t advice=ev_daylight_observe(&daylight_observer,&lux,now,occupancy,mode,
        virtual_lamp,EV_DAYLIGHT_SHARED_SENSOR,force_off);
    if(advice==EV_ON)virtual_lamp=1;
    if(advice==EV_OFF)virtual_lamp=0;
    taskENTER_CRITICAL();
    daylight_report.now=now;
    daylight_report.lux=daylight_observer.lux.filtered;
    daylight_report.valid=daylight_observer.lux.valid;
    daylight_report.stable=daylight_observer.lux.stable;
    daylight_report.occupancy=occupancy;
    daylight_report.advice=advice;
    daylight_report.virtual_state=virtual_lamp;
    taskEXIT_CRITICAL();
}
#endif

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
#ifdef ECOVISION_DAYLIGHT_SHADOW
    ev_core_init(&daylight_observer);
    virtual_lamp=0;
#endif
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
    input.air_valid=environment.air_valid;
    input.eco2_ppm=environment.eco2_ppm;
    input.tvoc_ppb=environment.tvoc_ppb;
    /* NUCLEO MB1137 USER B1 is PC13, active high. */
    input.button_pressed=HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)==GPIO_PIN_SET;
    taskENTER_CRITICAL();
    Control_Update(&policy,now,&input);
    choose_command(now);
    ControlOutput output=policy.output;
#ifdef ECOVISION_DAYLIGHT_SHADOW
    const bool fault_off=policy.fault_active&&now-policy.fault_at>=60000U;
#endif
    taskEXIT_CRITICAL();


    ac.fresh_temperature=Control_TemperatureFresh(&input);
    ac.fresh_positive=input.vision_valid && input.count>0U && input.count<=20U;
    ac.temperature_x100=environment.temp_x100;
    ac.humidity_x100=environment.humi_x100;
#ifdef ECOVISION_DAYLIGHT_SHADOW
    observe_daylight(&environment,&output,now,fault_off);
#endif
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

#ifdef ECOVISION_DAYLIGHT_SHADOW
/* Called by the sensor task. Serial logging stays outside the output task. */
void AppControl_LogDaylight(void) {
    taskENTER_CRITICAL();
    const uint32_t now=daylight_report.now;
    const int32_t lux=daylight_report.lux;
    const uint8_t valid=daylight_report.valid, stable=daylight_report.stable;
    const uint8_t occupancy=daylight_report.occupancy, advice=daylight_report.advice;
    const uint8_t virtual_state=daylight_report.virtual_state;
    const bool actual=applied.led;
    taskEXIT_CRITICAL();
    AppLog_Printf("[LUX_SHADOW] ms=%lu valid=%u stable=%u lux_x100=%ld occ=%u actual=%u virtual=%u advice=%u\r\n",
        (unsigned long)now,(unsigned)valid,(unsigned)stable,(long)lux,
        (unsigned)occupancy,(unsigned)actual,(unsigned)virtual_state,(unsigned)advice);
}
#endif

void AppControl_SafetyOff(void) {
    taskENTER_CRITICAL();
    if(policy.output.mode!=CONTROL_LOCAL_OFF)Control_LocalOff(&policy);
    taskEXIT_CRITICAL();
}

static uint8_t ir_ac_setpoint = AC_SETPOINT_C;

bool AppControl_ApplyLocalIr(uint8_t cmd) {
    uint32_t now = HAL_GetTick();
    bool ac_action = false;
    uint8_t ac_target = 0U;
    bool handled = false;
    bool locked = false;

    taskENTER_CRITICAL();
    if (policy.output.mode == CONTROL_LOCAL_OFF) {
        locked = true;
    } else {
        uint8_t fan = policy.output.fan_level;
        bool led = policy.output.led;
        uint32_t epoch = policy.output.control_epoch;
        switch (cmd) {
        case 0x45U: /* POWER: all off */
            handled = Control_Request(&policy, now, epoch, 120U, 0U, false);
            ac_action = true;
            ac_target = 0U;
            break;
        case 0x47U: /* MENU */
        case 0x43U: /* BACK (↩): restore AUTO */
            handled = Control_Request(&policy, now, epoch, 0U, 0U, false);
            ac.manual = false;
            break;
        case 0x15U: /* PLAY (▶): toggle LED */
            handled = Control_Request(&policy, now, epoch, 120U, fan, !led);
            break;
        case 0x4AU: /* 9: colorful rainbow LED ON */
            handled = Control_Request(&policy, now, epoch, 120U, fan, true);
            break;
        case 0x16U: /* 0: fan off */
            handled = Control_Request(&policy, now, epoch, 120U, 0U, led);
            break;
        case 0x0CU: /* 1 */
            handled = Control_Request(&policy, now, epoch, 120U, 1U, led);
            break;
        case 0x18U: /* 2 */
            handled = Control_Request(&policy, now, epoch, 120U, 2U, led);
            break;
        case 0x5EU: /* 3 */
            handled = Control_Request(&policy, now, epoch, 120U, 3U, led);
            break;
        case 0x08U: /* 4 */
            handled = Control_Request(&policy, now, epoch, 120U, 4U, led);
            break;
        case 0x1CU: /* 5 */
            handled = Control_Request(&policy, now, epoch, 120U, 5U, led);
            break;
        case 0x40U: /* + : fan +1 */
            if (fan < CONTROL_FAN_MAX) ++fan;
            handled = Control_Request(&policy, now, epoch, 120U, fan, led);
            break;
        case 0x19U: /* - : fan -1 */
            if (fan > 0U) --fan;
            handled = Control_Request(&policy, now, epoch, 120U, fan, led);
            break;
        case 0x0DU: /* C */
        case 0x5AU: /* 6: AC toggle */
            handled = true;
            ac_action = true;
            ac_target = ac.may_be_on ? 0U : ir_ac_setpoint;
            break;
        case 0x09U: /* >>| : AC temp +1 */
            if (ir_ac_setpoint < 30U) ++ir_ac_setpoint;
            handled = true;
            ac_action = true;
            ac_target = ir_ac_setpoint;
            break;
        case 0x07U: /* |<< : AC temp -1 */
            if (ir_ac_setpoint > 16U) --ir_ac_setpoint;
            handled = true;
            ac_action = true;
            ac_target = ir_ac_setpoint;
            break;
        default:
            break;
        }
        if (handled) {
            /* Physical IR actions bump control_epoch so stale in-flight web commands cannot override. */
            Control_BumpEpoch(&policy);
        }
    }
    taskEXIT_CRITICAL();

    if (locked) {
        AppLog_Printf("[IR-CTRL] REJECTED LOCAL_OFF cmd=0x%02X\r\n", (unsigned)cmd);
        return false;
    }
    if (ac_action && AppAcDemo_Ready()) {
        AcModuleResult res = ac_send(ac_target, "IR_LOCAL");
        if (res != AC_MODULE_REJECTED) {
            ac.manual = true;
            ac.manual_at = now;
            ac.epoch = policy.output.control_epoch;
            if (policy.output.occupancy == CONTROL_VACANT) ac.vacancy_attempted = true;
        }
    }
    if (handled) {
        AppLog_Printf("[IR-CTRL] APPLIED cmd=0x%02X fan=%u led=%u ac_set=%u epoch=%lu\r\n",
            (unsigned)cmd, (unsigned)policy.output.fan_level, policy.output.led ? 1U : 0U,
            ac_action ? (unsigned)ac_target : (ac.may_be_on ? (unsigned)ir_ac_setpoint : 0U),
            (unsigned long)policy.output.control_epoch);
    }
    return handled;
}
