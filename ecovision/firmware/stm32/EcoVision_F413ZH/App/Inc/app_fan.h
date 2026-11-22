#ifndef APP_FAN_H
#define APP_FAN_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

bool AppFan_Init(TIM_HandleTypeDef *timer);
bool AppFan_SetLevel(uint8_t level);
uint8_t AppFan_DutyPercent(uint8_t level);
uint8_t AppFan_GetLevel(void);

#endif
