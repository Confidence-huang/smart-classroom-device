#include "app_ir_rx.h"
#include "main.h"

enum { IR_RX_QUEUE_SIZE = 8U };

static IrNecDecoder decoder;
static IrNecEvent queue[IR_RX_QUEUE_SIZE];
static volatile uint8_t q_head, q_tail;
static volatile uint32_t edge_count;
static volatile uint32_t last_dt_us;
static volatile uint32_t last_edge_ts;
static volatile uint32_t mute_until_ms;

static uint32_t tim5_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    if (HAL_RCC_GetHCLKFreq() != pclk1) return pclk1 * 2U;
    return pclk1;
}

void AppIrRx_Init(void)
{
    IrNec_Init(&decoder);
    q_head = 0U;
    q_tail = 0U;
    edge_count = 0U;
    mute_until_ms = 0U;

    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1 = 0U;
    uint32_t clk = tim5_clock_hz();
    uint32_t psc = (clk >= 1000000U) ? (clk / 1000000U - 1U) : 0U;
    TIM5->PSC = psc;
    TIM5->ARR = 0xFFFFFFFFU;
    TIM5->CNT = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->CR1 = TIM_CR1_CEN;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_2;
    pin.Mode = GPIO_MODE_IT_FALLING;
    pin.Pull = GPIO_PULLUP;
    pin.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &pin);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

void AppIrRx_MuteMs(uint32_t ms)
{
    mute_until_ms = HAL_GetTick() + ms;
}

uint32_t AppIrRx_EdgeCount(void)
{
    return edge_count;
}

uint8_t AppIrRx_PinLevel(void)
{
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET ? 1U : 0U;
}

uint32_t AppIrRx_LastDtUs(void)
{
    return last_dt_us;
}

void AppIrRx_ExtiIrq(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_2) == RESET) return;
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);

    const uint32_t now_us = TIM5->CNT;
    if (edge_count > 0U) last_dt_us = (uint32_t)(now_us - last_edge_ts);
    last_edge_ts = now_us;
    ++edge_count;

    if (mute_until_ms != 0U && (int32_t)(HAL_GetTick() - mute_until_ms) < 0)
    {
        IrNec_Init(&decoder);
        return;
    }

    IrNecEvent ev;
    IrNecStatus st = IrNec_FeedFallingEdge(&decoder, now_us, &ev);
    if (st == IR_NEC_FRAME || st == IR_NEC_REPEAT)
    {
        uint8_t next = (uint8_t)((q_head + 1U) % IR_RX_QUEUE_SIZE);
        if (next != q_tail)
        {
            queue[q_head] = ev;
            q_head = next;
        }
    }
}

bool AppIrRx_Pop(IrNecEvent *out)
{
    if (!out || q_tail == q_head) return false;
    *out = queue[q_tail];
    q_tail = (uint8_t)((q_tail + 1U) % IR_RX_QUEUE_SIZE);
    return true;
}
