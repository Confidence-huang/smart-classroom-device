#include "app_light.h"
#include "stm32f4xx_hal.h"
extern ADC_HandleTypeDef hadc1;

void AppLight_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_5;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &pin);
}

bool AppLight_ReadRaw12(uint16_t *out)
{
    if (out == NULL) return false;
    ADC_ChannelConfTypeDef channel = {0};
    channel.Channel = ADC_CHANNEL_15;
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) return false;
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return false;
    }
    const bool ok = HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK;
    if (ok) *out = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return ok;
}
