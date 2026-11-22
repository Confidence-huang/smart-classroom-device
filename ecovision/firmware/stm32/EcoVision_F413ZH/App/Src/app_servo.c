/**
 * @file    app_servo.c
 * @brief   舵机 PWM：TIM1_CH1 / PE9，50 Hz，1 us 分辨率，寄存器级最小实现
 *
 * 与寄存器相关的关键点（RM0430 §18 / §6）：
 *   - TIM1 是高级定时器，除 CEN 外还必须置 BDTR.MOE=1 才有输出；
 *   - 定时器时钟：APB2 预分频=1 时 = PCLK2，否则 = PCLK2×2（本工程 96 MHz 时 = 96 MHz）；
 *   - 1 MHz 计数（PSC=时钟/1MHz-1）+ ARR=19999 → 20 ms 周期 = 50 Hz；
 *   - CCR1 写脉宽（us）；CCMR1 设为 PWM 模式 1 + 预装载，避免改脉宽时出现毛刺。
 *
 * 时钟自适应：上电读取 PCLK2 和 APB2 预分频，算出真实定时器时钟，
 * 所以无论 CubeMX 里改成 16 MHz、96 MHz 还是 100 MHz，脉宽都还是准的。
 */
#include "main.h"
#include "app_servo.h"

/* ---- 用到的寄存器位定义（等价于 HAL 里的 TIM_CCMR1_OC1M / TIM_CCER_CC1E 等） ---- */
#define SRV_CCMR1_CC1S_Msk      (0x3UL << 0)   /**< CC1S[1:0]：通道 1 配置为输出 */
#define SRV_CCMR1_OC1M_Msk      (0x7UL << 4)   /**< OC1M[2:0]：输出比较模式 */
#define SRV_CCMR1_OC1M_PWM1     (0x6UL << 4)   /**< 110 = PWM 模式 1 */
#define SRV_CCMR1_OC1PE         (0x1UL << 3)   /**< OC1PE：CCR1 预装载使能 */
#define SRV_CCER_CC1E           (0x1UL << 0)   /**< CC1E：通道 1 输出使能 */
#define SRV_CR1_ARPE            (0x1UL << 7)   /**< ARPE：ARR 预装载使能 */
#define SRV_CR1_CEN             (0x1UL << 0)   /**< CEN：计数器使能 */
#define SRV_EGR_UG              (0x1UL << 0)   /**< UG：软件产生更新事件（装载影子寄存器） */
#define SRV_BDTR_MOE            (0x1UL << 15)  /**< MOE：主输出使能（高级定时器必须） */

#define SERVO_TIM               TIM1
#define SERVO_GPIO_PORT         GPIOE
#define SERVO_GPIO_PIN          GPIO_PIN_9
#define SERVO_GPIO_AF           GPIO_AF1_TIM1

static uint16_t s_pulse_us = SERVO_DEFAULT_PULSE_US;

/** @brief 取 TIM1 的真实内核时钟（Hz）：APB2 分频不为 1 时定时器时钟 = PCLK2 × 2 */
static uint32_t Servo_GetTimerClockHz(void)
{
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;

    /* PPRE2 编码：0xx = /1，100 = /2，101 = /4，110 = /8，111 = /16 */
    return (ppre2 >= 4U) ? (pclk2 * 2U) : pclk2;
}

void Servo_Init(void)
{
    GPIO_InitTypeDef gpio = { 0 };
    uint32_t tim_clk_hz;
    uint32_t psc;

    /* 1) 时钟：GPIOE + TIM1 */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* 2) PE9 复用为 TIM1_CH1（AF1） */
    gpio.Pin       = SERVO_GPIO_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = SERVO_GPIO_AF;
    HAL_GPIO_Init(SERVO_GPIO_PORT, &gpio);

    /* 3) 时基：计数频率 1 MHz → 1 个 tick = 1 us；周期 20000 tick = 20 ms */
    tim_clk_hz = Servo_GetTimerClockHz();
    psc = tim_clk_hz / 1000000U;
    if (psc == 0U)
    {
        psc = 1U;
    }
    SERVO_TIM->PSC = (uint16_t)(psc - 1U);
    SERVO_TIM->ARR = SERVO_PERIOD_US - 1U;
    SERVO_TIM->CCR1 = s_pulse_us;

    /* 4) 通道 1：PWM 模式 1 + CCR 预装载 */
    {
        uint32_t ccmr1 = SERVO_TIM->CCMR1;
        ccmr1 &= ~(SRV_CCMR1_CC1S_Msk | SRV_CCMR1_OC1M_Msk);
        ccmr1 |= (SRV_CCMR1_OC1M_PWM1 | SRV_CCMR1_OC1PE);
        SERVO_TIM->CCMR1 = ccmr1;
    }
    SERVO_TIM->CCER |= SRV_CCER_CC1E;

    /* 5) 高级定时器：必须开主输出 MOE，否则 PB/PE 上一直无波形 */
    SERVO_TIM->BDTR |= SRV_BDTR_MOE;

    /* 6) 装载 PSC/ARR 影子寄存器，再启动计数 */
    SERVO_TIM->CR1 |= SRV_CR1_ARPE;
    SERVO_TIM->EGR  = SRV_EGR_UG;
    SERVO_TIM->CR1 |= SRV_CR1_CEN;
}

void Servo_SetPulseUs(uint16_t pulse_us)
{
    if (pulse_us < SERVO_MIN_PULSE_US)
    {
        pulse_us = SERVO_MIN_PULSE_US;
    }
    else if (pulse_us > SERVO_MAX_PULSE_US)
    {
        pulse_us = SERVO_MAX_PULSE_US;
    }

    s_pulse_us = pulse_us;
    SERVO_TIM->CCR1 = (uint32_t)pulse_us;   /* 1 tick = 1 us */
}

uint16_t Servo_GetPulseUs(void)
{
    return s_pulse_us;
}

void Servo_SetAngleDeg(uint16_t deg)
{
    uint32_t span_us = (uint32_t)(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US);  /* 2000 us */

    if (deg > 180U)
    {
        deg = 180U;
    }
    /* pulse = 500 + deg × 2000 / 180，先乘后除避免丢精度 */
    Servo_SetPulseUs((uint16_t)(SERVO_MIN_PULSE_US + (deg * span_us + 90U) / 180U));
}
