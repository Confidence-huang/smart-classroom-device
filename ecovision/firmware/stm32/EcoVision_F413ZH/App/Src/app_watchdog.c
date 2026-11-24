/* Single IWDG owner. Do not enable teammate AppHealth_Start concurrently. */
#include "app_watchdog.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_log.h"
static HealthPolicy health;
static bool started,reported,failed;
void AppWatchdog_Beat(unsigned id) {
 taskENTER_CRITICAL();Health_Beat(&health,id,HAL_GetTick());taskEXIT_CRITICAL();
}
void AppWatchdog_Delay(unsigned id,uint32_t ms) {
 while(ms){AppWatchdog_Beat(id);uint32_t step=ms>100U?100U:ms;osDelay(step);ms-=step;}
 AppWatchdog_Beat(id);
}
void AppWatchdog_Poll(void) {
 uint32_t now=HAL_GetTick();
 if(!reported){
  uint32_t flags=RCC->CSR;__HAL_RCC_CLEAR_RESET_FLAGS();reported=true;
  AppLog_Printf("[WDT] reset_flags=0x%08lx iwdg=%u; startup grace=8s\r\n",(unsigned long)flags,(flags&RCC_CSR_IWDGRSTF)?1U:0U);
 }
 if(failed)return;
 if(!started && now<8000U)return;
 taskENTER_CRITICAL();bool ready=Health_Ready(&health,now);taskEXIT_CRITICAL();
 if(!started) {
  /* Start even when a required task never ran: it must not bypass protection. */
  IWDG->KR=0xCCCCU;IWDG->KR=0x5555U;IWDG->PR=6U;IWDG->RLR=1999U;
  uint32_t at=HAL_GetTick();while(IWDG->SR){if((uint32_t)(HAL_GetTick()-at)>=100U){failed=true;return;}}
  started=true;AppLog_Printf("[WDT] started nominal=16s (LSI tolerance); health=%u\r\n",ready?1U:0U);
 }
 if(!ready){failed=true;AppLog_Printf("[WDT] task progress lost; stop feeding until reset\r\n");return;}
 IWDG->KR=0xAAAAU;
}
