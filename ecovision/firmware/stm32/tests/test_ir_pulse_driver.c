#include "app_ir_pulse.h"
#include "ir_pulse_codec.h"
#include "main.h"
#include "app_control.h"
#include "cmsis_os.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
MockTimer mock_tim1,mock_tim4,mock_tim5;
MockGpio mock_gpioa,mock_gpioe;MockRcc mock_rcc;
static uint32_t tick;
static uint8_t mode,queue[8];static unsigned qread,qwrite,qsize,completed,timeouts,rx_frames,learn_timeouts;
static jmp_buf task_exit;
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *c,uint32_t *l){c->APB1CLKDivider=2;c->APB2CLKDivider=1;*l=3;}
uint32_t HAL_RCC_GetPCLK1Freq(void){return 48000000U;}
uint32_t HAL_RCC_GetPCLK2Freq(void){return 96000000U;}
void HAL_GPIO_WritePin(MockGpio *p,uint32_t pin,uint32_t v){(void)p;(void)pin;(void)v;}
void HAL_GPIO_Init(MockGpio *p,GPIO_InitTypeDef *pin){(void)p;(void)pin;}
void HAL_NVIC_SetPriority(int irq,uint32_t p,uint32_t sub){assert((irq==30||irq==50)&&p==6&&sub==0);}
void HAL_NVIC_EnableIRQ(int irq){(void)irq;}
void HAL_NVIC_DisableIRQ(int irq){(void)irq;}
uint32_t HAL_GetTick(void){return tick;}
void AppControl_GetSnapshot(ControlOutput *s){s->mode=mode;}
osMessageQueueId_t osMessageQueueNew(uint32_t n,uint32_t size,const void *a){assert(n==8&&size==1);(void)a;return queue;}
int osMessageQueuePut(osMessageQueueId_t q,const void *p,uint8_t priority,uint32_t timeout){(void)priority;(void)timeout;assert(q==queue);if(qsize==8)return -1;queue[qwrite++%8]=*(const uint8_t *)p;qsize++;return osOK;}
int osMessageQueueGet(osMessageQueueId_t q,void *p,uint8_t *priority,uint32_t timeout){(void)priority;(void)timeout;assert(q==queue);if(!qsize)return -1;*(uint8_t *)p=queue[qread++%8];qsize--;return osOK;}
int osMessageQueueReset(osMessageQueueId_t q){assert(q==queue);qsize=0;qread=qwrite=0;return osOK;}
void osDelay(uint32_t ms){if(ms==800)return;assert(ms==2);tick+=ms;longjmp(task_exit,1);}
void AppLog_Printf(const char *fmt,...){
    if(strstr(fmt,"waveform completed"))completed++;
    if(strstr(fmt,"TX timeout"))timeouts++;
    if(strstr(fmt,"RX pulses="))rx_frames++;
    if(strstr(fmt,"learn timeout"))learn_timeouts++;
}
static void once(void){if(!setjmp(task_exit))AppIrPulse_Run(NULL);}
static void edge(uint32_t stamp,bool high){TIM5->CCR3=stamp;GPIOA->IDR=high?GPIO_PIN_2:0;TIM5->SR=TIM_SR_CC3IF;AppIrPulse_Timer5IRQ();}
int main(void){
    assert(!AppIrPulse_Request('s'));assert(AppIrPulse_Init());
    assert(TIM1->CCR2==0&&TIM1->ARR==2525&&TIM4->PSC==95&&TIM5->PSC==95);
    assert(!AppIrPulse_Request(0)&&!AppIrPulse_Request('z'));
    IrPulseFrame f;assert(IrPulse_Encode24(1,26,&f));
    assert(AppIrPulse_Request('s'));once();assert(TIM1->CCR2==1263&&TIM4->ARR==8999);
    for(unsigned i=1;i<f.len;i++){
        TIM4->SR=TIM_SR_UIF;AppIrPulse_Timer4IRQ();
        assert(TIM4->ARR==(uint32_t)f.us[i]-1U);
        assert(TIM1->CCR2==((i&1U)?0U:1263U));
    }
    TIM4->SR=TIM_SR_UIF;AppIrPulse_Timer4IRQ();once();
    assert(TIM1->CCR2==0&&TIM4->DIER==0&&completed==1);
    assert(AppIrPulse_Request('s'));once();mode=CONTROL_LOCAL_OFF;once();assert(TIM1->CCR2==0&&TIM4->DIER==0);
    assert(AppIrPulse_Request('s'));once();assert(qsize==0&&TIM1->CCR2==0);mode=CONTROL_AUTO;
    assert(AppIrPulse_Request('s'));once();tick+=1701;once();assert(timeouts==1&&TIM1->CCR2==0);
    assert(AppIrPulse_Request('l'));once();
    uint32_t stamp=30000;edge(stamp,false);
    for(unsigned i=0;i<f.len;i++){stamp+=f.us[i];edge(stamp,(i&1U)==0U);}
    TIM5->CNT=stamp+20001;once();assert(rx_frames==1);
    assert(AppIrPulse_Request('r'));once();assert(TIM1->CCR2==1263);
    assert(AppIrPulse_Request('x'));once();assert(TIM1->CCR2==0&&TIM4->DIER==0);
    assert(AppIrPulse_Request('l'));once();tick+=10001;once();assert(learn_timeouts==1);
    assert(AppIrPulse_Request('r'));once();assert(TIM1->CCR2==0);
    TIM5->SR=TIM_SR_CC3OF;AppIrPulse_Timer5IRQ();once();
    puts("PASS: init OFF, 51 TX segments, completion, LOCAL_OFF cancel/discard, TX timeout, captured demo, learn/replay/abort, learn timeout clears stale replay, overcapture recovery");
    return 0;
}
