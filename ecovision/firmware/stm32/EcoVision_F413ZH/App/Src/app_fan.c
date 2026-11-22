#include "app_fan.h"
#include "main.h"

static volatile uint8_t actual_level;
uint8_t AppFan_GetLevel(void){return actual_level;}
static TIM_HandleTypeDef *fan_timer;
static const uint8_t duty_percent[5] = {0U, 40U, 60U, 80U, 100U};

bool AppFan_Init(TIM_HandleTypeDef *timer)
{
    fan_timer = timer;
    HAL_GPIO_WritePin(FAN_STBY_GPIO_Port, FAN_STBY_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FAN_AIN1_GPIO_Port, FAN_AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FAN_AIN2_GPIO_Port, FAN_AIN2_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(timer, TIM_CHANNEL_2, 0U);
    if (HAL_TIM_PWM_Start(timer, TIM_CHANNEL_2) != HAL_OK)
    {
        fan_timer = NULL;
        return false;
    }
    actual_level=0U;
    return true;
}

uint8_t AppFan_DutyPercent(uint8_t level)
{
    return level < 5U ? duty_percent[level] : 0U;
}

bool AppFan_SetLevel(uint8_t level)
{
    if (fan_timer == NULL || level >= 5U)
    {
        return false;
    }
    if (level == 0U)
    {
        /* Standby makes both outputs high impedance; the motor coasts to rest. */
        HAL_GPIO_WritePin(FAN_STBY_GPIO_Port, FAN_STBY_Pin, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(fan_timer, TIM_CHANNEL_2, 0U);
        HAL_GPIO_WritePin(FAN_AIN1_GPIO_Port, FAN_AIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(FAN_AIN2_GPIO_Port, FAN_AIN2_Pin, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(FAN_AIN2_GPIO_Port, FAN_AIN2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(FAN_AIN1_GPIO_Port, FAN_AIN1_Pin, GPIO_PIN_SET);
        const uint32_t period = __HAL_TIM_GET_AUTORELOAD(fan_timer) + 1U;
        __HAL_TIM_SET_COMPARE(fan_timer, TIM_CHANNEL_2,
                              period * duty_percent[level] / 100U);
        HAL_GPIO_WritePin(FAN_STBY_GPIO_Port, FAN_STBY_Pin, GPIO_PIN_SET);
    }
    actual_level=level;
    return true;
}
