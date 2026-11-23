/* Port/review of wzy F407 IR package v0.3, adapted to F413 resources.
 * TIM1 CH2 PE11 carrier; TIM4 envelope; TIM5 CH3 PA2 active-low receiver.
 * IRQ priority 6: no FreeRTOS API, logging, decoding or frame memcpy in IRQ. */
#include "app_ir_pulse.h"
#include "ir_pulse_codec.h"
#include "main.h"
#include "app_control.h"
#include "app_log.h"
#include "cmsis_os.h"
#include <string.h>

enum { EDGE_COUNT=256, GAP_US=20000, LEARN_MS=10000, TX_TIMEOUT_MS=1700 };
typedef struct {uint32_t stamp;uint8_t high;} Edge;
static volatile Edge edges[EDGE_COUNT];
static volatile uint16_t head,tail;
static volatile bool rx_fault,tx_busy,tx_done;
static osMessageQueueId_t commands;
static IrPulseFrame transmit,received,learned;
static uint16_t tx_index;
static uint32_t carrier_compare,tx_started,last_edge,learn_started;
static bool capturing,frame_bad,previous_high,learning,learned_valid;

static void stop_tx(void) {
    TIM4->DIER=0;TIM4->CR1=0;TIM4->SR=0;TIM1->CCR2=0;
    tx_busy=false;tx_done=false;
}
static void arm_segment(uint16_t us) {
    TIM4->CR1=TIM_CR1_OPM;TIM4->ARR=(uint32_t)us-1U;TIM4->CNT=0;
    TIM4->EGR=TIM_EGR_UG;TIM4->SR=0;
    TIM4->DIER=TIM_DIER_UIE;TIM4->CR1=TIM_CR1_OPM|TIM_CR1_CEN;
}
void AppIrPulse_Timer4IRQ(void) {
    if(!(TIM4->SR&TIM_SR_UIF)||!(TIM4->DIER&TIM_DIER_UIE))return;
    TIM4->SR=~TIM_SR_UIF;
    if(!tx_busy){stop_tx();return;}
    ++tx_index;
    if(tx_index>=transmit.len) {
        TIM4->DIER=0;TIM4->CR1=0;TIM1->CCR2=0;
        tx_busy=false;tx_done=true;return;
    }
    TIM1->CCR2=(tx_index&1U)?0U:carrier_compare;
    arm_segment(transmit.us[tx_index]);
}
void AppIrPulse_Timer5IRQ(void) {
    const uint32_t status=TIM5->SR;
    if(status&TIM_SR_CC3OF){rx_fault=true;TIM5->SR=~TIM_SR_CC3OF;}
    if(!(status&TIM_SR_CC3IF))return;
    const uint32_t stamp=TIM5->CCR3;
    TIM5->SR=~TIM_SR_CC3IF;
    if(tx_busy)return; /* Do not learn this transmitter's own optical echo. */
    const uint16_t next=(uint16_t)((head+1U)&(EDGE_COUNT-1U));
    if(next==tail){rx_fault=true;return;}
    edges[head].stamp=stamp;
    edges[head].high=(GPIOA->IDR&GPIO_PIN_2)?1U:0U;
    __DMB();head=next;
}
bool AppIrPulse_Init(void) {
    if(commands)return true;
    RCC_ClkInitTypeDef clocks;uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks,&latency);
    const uint32_t apb1=HAL_RCC_GetPCLK1Freq()*(clocks.APB1CLKDivider==RCC_HCLK_DIV1?1U:2U);
    const uint32_t apb2=HAL_RCC_GetPCLK2Freq()*(clocks.APB2CLKDivider==RCC_HCLK_DIV1?1U:2U);
    if(apb1<1000000U||apb1%1000000U||apb2<1000000U)return false;
    /* Existing project uses default TIMPRE=0. Do not silently mis-time other clock trees. */
    if(RCC->DCKCFGR&RCC_DCKCFGR_TIMPRE)return false;
    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();__HAL_RCC_TIM4_CLK_ENABLE();__HAL_RCC_TIM5_CLK_ENABLE();
    __HAL_RCC_TIM1_FORCE_RESET();__HAL_RCC_TIM1_RELEASE_RESET();
    __HAL_RCC_TIM4_FORCE_RESET();__HAL_RCC_TIM4_RELEASE_RESET();
    __HAL_RCC_TIM5_FORCE_RESET();__HAL_RCC_TIM5_RELEASE_RESET();
    GPIO_InitTypeDef pin={0};
    HAL_GPIO_WritePin(GPIOE,GPIO_PIN_11,GPIO_PIN_RESET);
    const uint32_t period=(apb2+19000U)/38000U;
    TIM1->PSC=0;TIM1->ARR=period-1U;TIM1->CCR2=0;
    TIM1->CCMR1=6U<<12; /* PWM1 CH2, immediate CCR updates for envelope gating. */
    TIM1->CCER=TIM_CCER_CC2E;TIM1->BDTR=TIM_BDTR_MOE;
    TIM1->EGR=TIM_EGR_UG;TIM1->SR=0;TIM1->CR1=TIM_CR1_CEN;
    carrier_compare=period/2U;
    pin.Pin=GPIO_PIN_11;pin.Mode=GPIO_MODE_AF_PP;pin.Pull=GPIO_PULLDOWN;
    pin.Speed=GPIO_SPEED_FREQ_HIGH;pin.Alternate=GPIO_AF1_TIM1;HAL_GPIO_Init(GPIOE,&pin);
    TIM4->PSC=apb1/1000000U-1U;TIM4->ARR=9999;TIM4->EGR=TIM_EGR_UG;TIM4->SR=0;
    TIM5->PSC=apb1/1000000U-1U;TIM5->ARR=UINT32_MAX;
    TIM5->CCMR2=1U; /* CH3 direct input, no digital filter; gaps decoded by task. */
    TIM5->CCER=TIM_CCER_CC3E|TIM_CCER_CC3P|TIM_CCER_CC3NP;
    TIM5->EGR=TIM_EGR_UG;TIM5->SR=0;TIM5->DIER=TIM_DIER_CC3IE;TIM5->CR1=TIM_CR1_CEN;
    pin.Pin=GPIO_PIN_2;pin.Mode=GPIO_MODE_AF_PP;pin.Pull=GPIO_PULLUP;
    pin.Alternate=GPIO_AF2_TIM5;HAL_GPIO_Init(GPIOA,&pin);
    commands=osMessageQueueNew(8U,sizeof(uint8_t),NULL);
    if(!commands){TIM1->CCR2=0;TIM5->DIER=0;return false;}
    HAL_NVIC_SetPriority(TIM4_IRQn,6,0);HAL_NVIC_SetPriority(TIM5_IRQn,6,0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);HAL_NVIC_EnableIRQ(TIM5_IRQn);
    return true;
}
bool AppIrPulse_Request(uint8_t command) {
    return commands&&strchr("so+-mflrx",(int)command)!=NULL&&command!=0U&&
           osMessageQueuePut(commands,&command,0,0)==osOK;
}
static bool start_tx(const IrPulseFrame *frame) {
    if(tx_busy||!IrPulse_ValidRaw(frame))return false;
    memcpy(&transmit,frame,sizeof transmit);
    HAL_NVIC_DisableIRQ(TIM5_IRQn);tail=head;rx_fault=false;HAL_NVIC_EnableIRQ(TIM5_IRQn);
    capturing=false;tx_index=0;tx_done=false;tx_started=HAL_GetTick();
    tx_busy=true;TIM1->CCR2=carrier_compare;arm_segment(transmit.us[0]);return true;
}
static void finish_rx(void) {
    if(!capturing)return;
    capturing=false;
    if(frame_bad||received.len<20U||!IrPulse_ValidRaw(&received)) {
        AppLog_Printf("[IRP] RX discarded pulses=%u\r\n",(unsigned)received.len);return;
    }
    uint8_t cmd=0,param=0;
    bool decoded=IrPulse_Decode24(&received,&cmd,&param);
    if(learning){learned=received;learned_valid=true;learning=false;}
    AppLog_Printf("[IRP] RX pulses=%u demo24=%u cmd=0x%02X param=%u learned=%u\r\n",
        (unsigned)received.len,decoded?1U:0U,(unsigned)cmd,(unsigned)param,learned_valid?1U:0U);
}
static void poll_rx(void) {
    if(rx_fault) {
        HAL_NVIC_DisableIRQ(TIM5_IRQn);tail=head;rx_fault=false;HAL_NVIC_EnableIRQ(TIM5_IRQn);
        capturing=false;AppLog_Printf("[IRP] RX overflow/overcapture; frame discarded\r\n");return;
    }
    while(tail!=head) {
        const Edge e=edges[tail];__DMB();tail=(uint16_t)((tail+1U)&(EDGE_COUNT-1U));
        if(capturing&&(uint32_t)(e.stamp-last_edge)>=GAP_US)finish_rx();
        if(!capturing) {
            if(e.high)continue;
            capturing=true;received.len=0;frame_bad=false;
        } else {
            const uint32_t us=e.stamp-last_edge;
            if(e.high==previous_high||us<100U||us>15000U||received.len>=IR_PULSE_CAPACITY)frame_bad=true;
            else received.us[received.len++]=(uint16_t)us;
        }
        previous_high=e.high!=0;last_edge=e.stamp;
    }
    /* Mask capture briefly to make the quiet-tail decision atomic with enqueue. */
    HAL_NVIC_DisableIRQ(TIM5_IRQn);
    const bool quiet=tail==head&&capturing&&(uint32_t)(TIM5->CNT-last_edge)>=GAP_US;
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
    if(quiet)finish_rx();
}
static void handle_request(uint8_t key) {
    static IrPulseFrame demo;
    if(key=='x'){stop_tx();learning=false;AppLog_Printf("[IRP] aborted\r\n");return;}
    if(tx_busy){AppLog_Printf("[IRP] rejected busy\r\n");return;}
    if(key=='l') {
        learning=true;learned_valid=false;learn_started=HAL_GetTick();capturing=false;
        AppLog_Printf("[IRP] learning one frame; timeout 10s\r\n");return;
    }
    bool started=false;
    if(key=='r')started=learned_valid&&start_tx(&learned);
    else {
        const uint8_t cmd=key=='s'?1U:key=='o'?2U:key=='+'?5U:key=='-'?6U:key=='m'?7U:key=='f'?8U:0U;
        started=IrPulse_Encode24(cmd,key=='s'?26U:0U,&demo)&&start_tx(&demo);
    }
    AppLog_Printf(started?"[IRP] TX started; no device confirmation\r\n":"[IRP] TX rejected/no learned frame\r\n");
}
void AppIrPulse_Run(void *argument) {
    (void)argument;osDelay(800U);
    AppLog_Printf("[IRP] READY TX=PE11 D5 RX=PA2 A8; Is/Io/I+/I-/Im/If/Il/Ir/Ix\r\n");
    for(;;) {
        ControlOutput control;AppControl_GetSnapshot(&control);
        if(control.mode==CONTROL_LOCAL_OFF) {
            if(tx_busy) { stop_tx(); }
            learning=false;
            (void)osMessageQueueReset(commands);
        } else {
            if(tx_done){tx_done=false;AppLog_Printf("[IRP] waveform completed; target execution unconfirmed\r\n");}
            uint8_t key;if(osMessageQueueGet(commands,&key,NULL,0)==osOK)handle_request(key);
        }
        if(tx_busy&&(uint32_t)(HAL_GetTick()-tx_started)>TX_TIMEOUT_MS) {
            stop_tx();AppLog_Printf("[IRP] TX timeout; carrier OFF\r\n");
        }
        if(learning&&(uint32_t)(HAL_GetTick()-learn_started)>=LEARN_MS) {
            learning=false;AppLog_Printf("[IRP] learn timeout\r\n");
        }
        poll_rx();osDelay(2U);
    }
}
