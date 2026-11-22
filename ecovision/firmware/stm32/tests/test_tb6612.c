#include "app_fan.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>

int mock_port;
static uint16_t outputs;
static uint16_t first_write;
static unsigned write_count;
static HAL_StatusTypeDef start_result = HAL_OK;

void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState value)
{
    assert(port == &mock_port);
    if (write_count++ == 0U)
    {
        first_write = pin;
    }
    outputs = value == GPIO_PIN_SET ? (uint16_t)(outputs | pin) : (uint16_t)(outputs & ~pin);
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
    assert(timer != NULL && channel == TIM_CHANNEL_2);
    return start_result;
}

int main(void)
{
    TIM_HandleTypeDef timer = {4799U, 999U};
    const uint32_t expected[] = {0U, 1920U, 2880U, 3840U, 4800U};
    outputs = 7U;
    assert(!AppFan_SetLevel(1U));
    assert(AppFan_Init(&timer));
    assert(timer.compare == 0U && outputs == 0U);
    assert(first_write == FAN_STBY_Pin);
    for (uint8_t level = 1U; level < 5U; ++level)
    {
        assert(AppFan_SetLevel(level));
        assert(timer.compare == expected[level]);
        assert(outputs == (FAN_STBY_Pin | FAN_AIN1_Pin));
        assert(AppFan_DutyPercent(level) == 20U * (level + 1U));
    }
    const unsigned previous_count = write_count;
    assert(!AppFan_SetLevel(5U));
    assert(write_count == previous_count && timer.compare == 4800U);
    write_count = 0U;
    assert(AppFan_SetLevel(0U));
    assert(first_write == FAN_STBY_Pin && outputs == 0U && timer.compare == 0U);
    start_result = HAL_ERROR;
    assert(!AppFan_Init(&timer));
    assert(!AppFan_SetLevel(1U) && outputs == 0U);
    puts("PASS: default standby, four PWM duties, direction, invalid level, standby-first stop, start failure");
    return 0;
}
