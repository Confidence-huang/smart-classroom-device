#ifndef APP_IR_H
#define APP_IR_H

#include "stm32f4xx_hal.h"

/* Call once from the dedicated low-priority IR task. */
void AppIr_Run(UART_HandleTypeDef *module, UART_HandleTypeDef *console);

#endif
