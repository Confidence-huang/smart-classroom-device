#ifndef APP_WATCHDOG_H
#define APP_WATCHDOG_H
#include "health_policy.h"
void AppWatchdog_Beat(unsigned);
void AppWatchdog_Delay(unsigned,uint32_t ms);
void AppWatchdog_Poll(void);
#endif
