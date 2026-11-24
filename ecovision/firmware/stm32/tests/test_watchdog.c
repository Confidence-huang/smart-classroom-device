#include "app_watchdog.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
MockIwdg mock_iwdg;MockRcc mock_rcc;unsigned critical_depth;
static uint32_t tick,max_delay;static unsigned reset_logs;
uint32_t HAL_GetTick(void){return tick;}
void osDelay(uint32_t ms){if(ms>max_delay)max_delay=ms;tick+=ms;}
void AppLog_Printf(const char *fmt,...){if(strstr(fmt,"reset_flags"))reset_logs++;}
static void all(void){for(unsigned i=0;i<HEALTH_COUNT;i++)AppWatchdog_Beat(i);}
int main(void){
 mock_rcc.CSR=RCC_CSR_IWDGRSTF;AppWatchdog_Poll();assert(reset_logs==1&&mock_rcc.CSR==0&&mock_iwdg.KR==0);
 tick=7999;all();AppWatchdog_Poll();assert(mock_iwdg.KR==0);
 tick=8000;all();AppWatchdog_Poll();assert(mock_iwdg.KR==0xAAAA&&mock_iwdg.PR==6&&mock_iwdg.RLR==1999);
 AppWatchdog_Delay(HEALTH_NET,60000);assert(max_delay==100&&tick==68000);
 mock_iwdg.KR=123;AppWatchdog_Poll();assert(mock_iwdg.KR==123); /* other tasks stalled, no feed */
 all();AppWatchdog_Poll();assert(mock_iwdg.KR==123&&!critical_depth); /* fault stays latched */
 puts("PASS: actual IWDG reset cause, grace, registers, chunked network wait, stalled-task stop feed and latch");return 0;
}
