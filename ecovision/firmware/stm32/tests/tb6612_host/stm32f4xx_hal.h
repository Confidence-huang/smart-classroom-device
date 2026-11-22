#ifndef TEST_STM32_HAL_H
#define TEST_STM32_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t period; uint32_t compare; } TIM_HandleTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;

#define TIM_CHANNEL_2 4U
#define __HAL_TIM_SET_COMPARE(timer, channel, value) ((timer)->compare = (value))
#define __HAL_TIM_GET_AUTORELOAD(timer) ((timer)->period)
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState value);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel);

#endif
