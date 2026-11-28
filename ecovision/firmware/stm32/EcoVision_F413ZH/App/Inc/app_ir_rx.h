#ifndef APP_IR_RX_H
#define APP_IR_RX_H

#include "ir_nec_codec.h"
#include <stdbool.h>
#include <stdint.h>

/* 38 kHz IR receiver OUT on PA2 (A8 / CN10-11), TIM5 1 MHz timebase, EXTI2 falling edge. */
void AppIrRx_Init(void);
void AppIrRx_ExtiIrq(void);
bool AppIrRx_Pop(IrNecEvent *out);
void AppIrRx_MuteMs(uint32_t ms);
uint32_t AppIrRx_EdgeCount(void);
uint8_t AppIrRx_PinLevel(void);
uint32_t AppIrRx_LastDtUs(void);

#endif
