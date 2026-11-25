/* UART5 access is initialized before scheduling and used by the output task. */
#ifndef APP_AC_DEMO_H
#define APP_AC_DEMO_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { AC_MODULE_ACK, AC_MODULE_REJECTED, AC_MODULE_UNKNOWN } AcModuleResult;
void AppAcDemo_Init(void);
bool AppAcDemo_Ready(void);
AcModuleResult AppAcDemo_Send(uint8_t temperature_c);
#endif
