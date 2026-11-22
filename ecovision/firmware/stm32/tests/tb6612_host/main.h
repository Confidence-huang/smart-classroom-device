#include "stm32f4xx_hal.h"

extern int mock_port;
#define FAN_STBY_GPIO_Port (&mock_port)
#define FAN_AIN1_GPIO_Port (&mock_port)
#define FAN_AIN2_GPIO_Port (&mock_port)
#define FAN_STBY_Pin 1U
#define FAN_AIN1_Pin 2U
#define FAN_AIN2_Pin 4U
