#include "app_uart_dma.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Stream stream1,stream5;unsigned critical_depth;
static uint8_t output[2048];static unsigned used,net_bytes,irq_calls,errors;static bool fail_start;
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *d){assert(d->Init.Mode==DMA_CIRCULAR&&d->Init.Direction==DMA_PERIPH_TO_MEMORY);return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u){u->ErrorCode=HAL_UART_ERROR_NONE;return HAL_OK;}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *u,uint8_t *b,uint16_t n){assert(critical_depth);u->buffer=b;u->capacity=n;u->hdmarx->Instance->count=n;return fail_start?HAL_ERROR:HAL_OK;}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *d){assert(d->Parent);irq_calls++;d->Instance->flags=0;}
void HAL_NVIC_SetPriority(IRQn_Type i,unsigned p,unsigned s){assert((i==15||i==21)&&p==6&&s==0);}
void HAL_NVIC_EnableIRQ(IRQn_Type i){(void)i;}
uint32_t HAL_GetTick(void){return 42;}
void AppVision_RxBytes(const uint8_t *b,uint16_t n,uint32_t at){assert(at==42&&used+n<=sizeof output);memcpy(output+used,b,n);used+=n;}
void AppNet_RxBytes(const uint8_t *b,uint16_t n,uint32_t at){assert(at==42);(void)b;net_bytes+=n;}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *u){assert(u->ErrorCode&HAL_UART_ERROR_DMA);errors++;}
static void event(UART_HandleTypeDef *u,unsigned position,unsigned stale_size){u->hdmarx->Instance->count=u->capacity-position;HAL_UARTEx_RxEventCallback(u,(uint16_t)stale_size);}
int main(void){
 UART_HandleTypeDef u={0},net={0},other={0};u.Instance=USART2;net.Instance=USART6;
 assert(!AppUartDma_Start(&other));assert(AppUartDma_Start(&u));assert(AppUartDma_Start(&net));
 assert(u.hdmarx->Instance==DMA1_Stream5&&u.hdmarx->Init.Channel==4);
 assert(net.hdmarx->Instance==DMA2_Stream1&&net.hdmarx->Init.Channel==5);
 for(unsigned i=0;i<512;i++)u.buffer[i]=(uint8_t)i;
 event(&u,256,256);event(&u,256,256);assert(used==256);
 event(&u,300,256);assert(used==300); /* late HT must read NDTR, not stale Size */
 event(&u,0,512);assert(used==512);
 event(&u,20,20);assert(used==532);
 for(unsigned i=0;i<used;i++)assert(output[i]==(uint8_t)i);
 event(&net,10,10);assert(net_bytes==10&&used==532);
 assert(AppUartDma_Start(&u));event(&u,5,5);assert(used==537);
 AppUartDma_Irq(0);AppUartDma_Irq(1);AppUartDma_Irq(2);assert(irq_calls==2);
 /* IDLE may run before DMA when both boundaries are pending. */
 stream5.flags=3;event(&u,24,24);assert(errors==1 && used==537);
 AppUartDma_Irq(0);assert(errors==1);
 unsigned before=used;event(&u,15,15);assert(used==before);
 assert(AppUartDma_Start(&u));event(&u,1,1);assert(used==before+1);
 fail_start=true;assert(!AppUartDma_Start(&net));assert(!critical_depth);
 puts("PASS: actual DMA adapter mappings, NDTR spans, duplicate and delayed events, wrap, isolation, restart failure");return 0;
}
