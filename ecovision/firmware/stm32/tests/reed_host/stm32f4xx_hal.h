#ifndef REED_HOST_HAL_H
#define REED_HOST_HAL_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
#define GPIOE ((void *)1)
#define GPIO_PIN_13 ((uint16_t)8192)
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
uint32_t HAL_GetTick(void);
#endif
