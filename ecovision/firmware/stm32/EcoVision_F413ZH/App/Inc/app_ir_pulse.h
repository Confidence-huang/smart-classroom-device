#ifndef APP_IR_PULSE_H
#define APP_IR_PULSE_H
#include <stdbool.h>
#include <stdint.h>
/* Separate pulse modules, NOT YS-IRTM UART. Call after osKernelInitialize. */
bool AppIrPulse_Init(void);
void AppIrPulse_Run(void *argument);
/* Task context; a true result only means queued, not emitted. */
bool AppIrPulse_Request(uint8_t command);
void AppIrPulse_Timer4IRQ(void);
void AppIrPulse_Timer5IRQ(void);
#endif
