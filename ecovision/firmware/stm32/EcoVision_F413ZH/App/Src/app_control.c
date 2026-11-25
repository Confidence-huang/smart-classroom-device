#include "app_control.h"
#include "app_environment.h"
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
static char last_link[17],last_boot[17];
static uint32_t last_seq,last_errors,last_rejected;
static uint32_t last_invalid_frames; /* Last interruption already handled by AUTO. */
static bool have_seq;

void AppControl_Init(void) {
#ifdef ECOVISION_DAYLIGHT_SHADOW
    ev_core_init(&daylight_observer);
    virtual_lamp=0;
#endif
    Control_Init(&policy);applied=policy.output;
    memset(last_link,0,sizeof last_link);memset(last_boot,0,sizeof last_boot);
    have_seq=false;last_errors=0;last_rejected=0;last_invalid_frames=0;
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
    ControlOutput output=policy.output;
#ifdef ECOVISION_DAYLIGHT_SHADOW
    const bool fault_off=policy.fault_active&&now-policy.fault_at>=60000U;
#endif
    taskEXIT_CRITICAL();
#ifdef ECOVISION_DAYLIGHT_SHADOW
    observe_daylight(&environment,&output,now,fault_off);
#endif
    return output;
}

void AppControl_PublishApplied(const ControlOutput *output) {
    taskENTER_CRITICAL();applied=*output;taskEXIT_CRITICAL();
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
