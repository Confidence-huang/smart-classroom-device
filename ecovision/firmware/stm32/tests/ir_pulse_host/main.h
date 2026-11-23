#ifndef IR_PULSE_HOST_MAIN_H
#define IR_PULSE_HOST_MAIN_H
#include <stdint.h>
#include <string.h>
typedef struct { volatile uint32_t CR1,PSC,ARR,CCR2,CCR3,CCMR1,CCMR2,CCER,BDTR,EGR,SR,DIER,CNT; } MockTimer;
typedef struct { volatile uint32_t IDR; } MockGpio;
typedef struct { volatile uint32_t DCKCFGR; } MockRcc;
extern MockTimer mock_tim1,mock_tim4,mock_tim5;
extern MockGpio mock_gpioa,mock_gpioe;
extern MockRcc mock_rcc;
#define TIM1 (&mock_tim1)
#define TIM4 (&mock_tim4)
#define TIM5 (&mock_tim5)
#define GPIOA (&mock_gpioa)
#define GPIOE (&mock_gpioe)
#define RCC (&mock_rcc)
#define GPIO_PIN_2 (1U<<2)
#define GPIO_PIN_11 (1U<<11)
#define GPIO_PIN_RESET 0U
#define GPIO_MODE_AF_PP 2U
#define GPIO_PULLUP 1U
#define GPIO_PULLDOWN 2U
#define GPIO_SPEED_FREQ_HIGH 2U
#define GPIO_AF1_TIM1 1U
#define GPIO_AF2_TIM5 2U
#define TIM_CR1_OPM 8U
#define TIM_CR1_CEN 1U
#define TIM_EGR_UG 1U
#define TIM_DIER_UIE 1U
#define TIM_DIER_CC3IE 8U
#define TIM_SR_UIF 1U
#define TIM_SR_CC3IF 8U
#define TIM_SR_CC3OF 2048U
#define TIM_CCER_CC2E 16U
#define TIM_CCER_CC3E 256U
#define TIM_CCER_CC3P 512U
#define TIM_CCER_CC3NP 2048U
#define TIM_BDTR_MOE 32768U
#define RCC_DCKCFGR_TIMPRE (1U<<24)
#define RCC_HCLK_DIV1 1U
#define TIM4_IRQn 30
#define TIM5_IRQn 50
#define __DMB() ((void)0)
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM4_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM5_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_FORCE_RESET() memset(TIM1,0,sizeof *TIM1)
#define __HAL_RCC_TIM4_FORCE_RESET() memset(TIM4,0,sizeof *TIM4)
#define __HAL_RCC_TIM5_FORCE_RESET() memset(TIM5,0,sizeof *TIM5)
#define __HAL_RCC_TIM1_RELEASE_RESET() ((void)0)
#define __HAL_RCC_TIM4_RELEASE_RESET() ((void)0)
#define __HAL_RCC_TIM5_RELEASE_RESET() ((void)0)
typedef struct { uint32_t APB1CLKDivider,APB2CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *,uint32_t *);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetPCLK2Freq(void);
void HAL_GPIO_WritePin(MockGpio *,uint32_t,uint32_t);
void HAL_GPIO_Init(MockGpio *,GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(int,uint32_t,uint32_t);
void HAL_NVIC_EnableIRQ(int);
void HAL_NVIC_DisableIRQ(int);
uint32_t HAL_GetTick(void);
#endif
