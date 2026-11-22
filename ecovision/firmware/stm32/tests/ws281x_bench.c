/* Standalone LED bring-up image. TIM2 and DMA1 Stream7 are bench-only. */
#include "main.h"

#include <stdbool.h>

enum {
    LED_COUNT = 16,
    LED_BITS = 24,
    PWM_PERIOD = 120,  /* 96 MHz / 120 = 800 kHz */
    ZERO_PULSE = 37,   /* 0.385 us high, 0.865 us low */
    ONE_PULSE = 70,    /* 0.729 us high, 0.521 us low */
    RESET_SLOTS = 320  /* 400 us low after the data */
};

static TIM_HandleTypeDef timer;
static DMA_HandleTypeDef stream;
static uint32_t duty[LED_COUNT * LED_BITS + RESET_SLOTS];

bool Ws281xBench_Init(void);
bool Ws281xBench_Show(bool first_green);
bool Ws281xBench_Write(const uint8_t frame[LED_COUNT][3]);

bool Ws281xBench_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    timer.Instance = TIM2;
    timer.Init.Period = PWM_PERIOD - 1U;
    TIM_OC_InitTypeDef channel = {0};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    if (HAL_TIM_PWM_Init(&timer) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&timer, &channel, TIM_CHANNEL_4) != HAL_OK ||
        HAL_TIM_PWM_Start(&timer, TIM_CHANNEL_4) != HAL_OK)
    {
        return false;
    }

    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_11;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Speed = GPIO_SPEED_FREQ_HIGH;
    pin.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOB, &pin);

    stream.Instance = DMA1_Stream7;
    stream.Init.Channel = DMA_CHANNEL_3;
    stream.Init.Direction = DMA_MEMORY_TO_PERIPH;
    stream.Init.MemInc = DMA_MINC_ENABLE;
    stream.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    stream.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    stream.Init.Mode = DMA_NORMAL;
    stream.Init.Priority = DMA_PRIORITY_HIGH;
    return HAL_DMA_Init(&stream) == HAL_OK;
}

bool Ws281xBench_Show(bool first_green)
{
    uint8_t frame[LED_COUNT][3] = {0};
    frame[0][0] = first_green ? 8U : 0U;
    return Ws281xBench_Write(frame);
}

bool Ws281xBench_Write(const uint8_t frame[LED_COUNT][3])
{
    unsigned bit = 0;
    for (unsigned pixel = 0; pixel < LED_COUNT; ++pixel)
    {
        for (unsigned color = 0; color < 3; ++color)
        {
            /* GRB is sent most significant bit first. */
            for (unsigned mask = 0x80U; mask != 0U; mask >>= 1)
            {
                duty[bit++] = (frame[pixel][color] & mask) != 0U ? ONE_PULSE : ZERO_PULSE;
            }
        }
    }
    if (HAL_DMA_Start(&stream, (uint32_t)duty, (uint32_t)&TIM2->CCR4,
                      (uint32_t)(sizeof duty / sizeof duty[0])) != HAL_OK)
    {
        return false;
    }

    __HAL_TIM_ENABLE_DMA(&timer, TIM_DMA_UPDATE);
    const HAL_StatusTypeDef result = HAL_DMA_PollForTransfer(&stream, HAL_DMA_FULL_TRANSFER, 5U);
    __HAL_TIM_DISABLE_DMA(&timer, TIM_DMA_UPDATE);
    if (result != HAL_OK)
    {
        (void)HAL_DMA_Abort(&stream);
        __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_4, 0U);
    }
    return result == HAL_OK;
}
