/* Event producers never access UART6. The network task owns all modem AT. */
#include "app_alert.h"
#include "alert_policy.h"
#include "app_log.h"
#include "main.h"
#include "cmsis_os.h"
static volatile unsigned pending;
static AlertPolicy policy;
static volatile uint32_t beats[HEALTH_COUNT];
static bool watchdog_started, failed;

void AppAlert_Init(void) {
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))pending|=ALERT_SYSTEM;
    __HAL_RCC_CLEAR_RESET_FLAGS();
}
void AppAlert_Control(bool vision, bool automatic, bool remote, uint8_t fan, uint8_t led) {
    unsigned events=AlertPolicy_Update(&policy,HAL_GetTick(),vision,automatic,remote,fan,led);
    taskENTER_CRITICAL();
    if(vision)pending&=~ALERT_VISION;
    if(!automatic)pending&=~ALERT_AUTO;
    pending|=events;
    taskEXIT_CRITICAL();
}
unsigned AppAlert_Take(void) {
    unsigned result=0;
    taskENTER_CRITICAL();
    if(pending&ALERT_SYSTEM)result=ALERT_SYSTEM;
    else if(pending&ALERT_VISION)result=ALERT_VISION;
    else if(pending&ALERT_CLOUD)result=ALERT_CLOUD;
    else if(pending&ALERT_AUTO)result=ALERT_AUTO;
    pending&=~result;
    taskEXIT_CRITICAL();
    return result;
}
void AppAlert_Retry(unsigned event) {
    taskENTER_CRITICAL();
    if(event!=ALERT_VISION || policy.vision_fault)pending|=event;
    taskEXIT_CRITICAL();
}
void AppHealth_Beat(unsigned task) {
    if(task<HEALTH_COUNT)beats[task]=HAL_GetTick();
}
void AppHealth_Start(void) {
    for(unsigned i=0;i<HEALTH_COUNT;i++)beats[i]=HAL_GetTick();
    /* Same programming sequence as STM32F4 HAL IWDG: /256, 2000 LSI cycles.
     * Nominal timeout is 16 s at 32 kHz; actual LSI tolerance applies. */
    IWDG->KR=0xCCCCu;
    IWDG->KR=0x5555u;
    IWDG->PR=6u;
    IWDG->RLR=1999u;
    uint32_t start=HAL_GetTick();
    while(IWDG->SR) {
        if((uint32_t)(HAL_GetTick()-start)>=100u) {
            failed=true;pending|=ALERT_SYSTEM;return;
        }
    }
    IWDG->KR=0xAAAAu;
    watchdog_started=true;
}
void AppHealth_Poll(void) {
    if(!watchdog_started || failed)return;
    uint32_t now=HAL_GetTick();
    for(unsigned i=0;i<HEALTH_COUNT;i++) {
        if(AlertHealth_Stale(now,beats[i])) {
            failed=true;
            AppAlert_Retry(ALERT_SYSTEM);
            AppLog_Printf("[HEALTH] task=%u stalled; IWDG recovery pending\r\n",i);
            return;
        }
    }
    IWDG->KR=0xAAAAu;
}
