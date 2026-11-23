#ifndef CONTROL_HOST_HAL_H
#define CONTROL_HOST_HAL_H
#include <stdint.h>
typedef struct {uint32_t ErrorCode;} UART_HandleTypeDef;
typedef enum {GPIO_PIN_RESET=0,GPIO_PIN_SET=1} GPIO_PinState;
#define GPIOC ((void *)3)
#define GPIO_PIN_13 ((uint16_t)(1U<<13))
uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(void *port,uint16_t pin);
#endif
