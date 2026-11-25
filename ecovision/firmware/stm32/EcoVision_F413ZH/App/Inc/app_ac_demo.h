/* UART5 starts before scheduling. The output task owns discovery and transmission at runtime. */
#ifndef APP_AC_DEMO_H
#define APP_AC_DEMO_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { AC_MODULE_ACK, AC_MODULE_REJECTED, AC_MODULE_UNKNOWN } AcModuleResult;
void AppAcDemo_Init(void);
void AppAcDemo_Poll(uint32_t now);
bool AppAcDemo_Ready(void);
AcModuleResult AppAcDemo_Send(uint8_t temperature_c);
#endif
