/* Only this adapter owns UART2/6 RX DMA. No allocation, parsing or logs in IRQ.
 * F413 SRAM is DMA-accessible, no D-cache. HT/TC/IDLE IRQs all priority 6.
 * Service latency must stay below 22 ms (256 bytes at 115200 8N1). */
#include "app_uart_dma.h"
#include "rx_cursor.h"
#include "app_net.h"
#include "app_vision.h"
#include <stddef.h>
#include "cmsis_os.h"
enum { CAP=512 };
typedef struct { DMA_HandleTypeDef dma;UART_HandleTypeDef *uart;RxCursor cursor;uint8_t data[CAP];bool configured;volatile bool discard; } Receiver;
static Receiver receivers[2];
static Receiver *find(UART_HandleTypeDef *u) {
 if(!u)return NULL;
 if(u->Instance==USART2)return &receivers[0];
 if(u->Instance==USART6)return &receivers[1];
 return NULL;
}
void AppUartDma_Stop(UART_HandleTypeDef *u) {
 Receiver *r=find(u);
 if(!r)return;
 /* Stop delivering bytes before the HAL disables the old transfer. */
 r->discard=true;
 (void)HAL_UART_AbortReceive(u);
}
bool AppUartDma_Start(UART_HandleTypeDef *u) {
 Receiver *r=find(u);if(!r)return false;
 /* Abort and reinitialize with the HAL tick available for timeouts. */
 r->discard=true;
 if(!r->configured) {
  __HAL_RCC_DMA1_CLK_ENABLE();__HAL_RCC_DMA2_CLK_ENABLE();
  r->dma.Instance=u->Instance==USART2?DMA1_Stream5:DMA2_Stream1;
  r->dma.Init.Channel=u->Instance==USART2?DMA_CHANNEL_4:DMA_CHANNEL_5;
  r->dma.Init.Direction=DMA_PERIPH_TO_MEMORY;r->dma.Init.PeriphInc=DMA_PINC_DISABLE;
  r->dma.Init.MemInc=DMA_MINC_ENABLE;r->dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
  r->dma.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;r->dma.Init.Mode=DMA_CIRCULAR;
  r->dma.Init.Priority=DMA_PRIORITY_HIGH;r->dma.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
 }
 if(HAL_UART_AbortReceive(u)!=HAL_OK || HAL_DMA_Init(&r->dma)!=HAL_OK)return false;
 r->uart=u;
 __HAL_LINKDMA(u,hdmarx,r->dma);

 /* Mask both event sources until the new cursor and receiver are ready. */
 taskENTER_CRITICAL();
 if(!r->configured) {
  const IRQn_Type irq=u->Instance==USART2?DMA1_Stream5_IRQn:DMA2_Stream1_IRQn;
  HAL_NVIC_SetPriority(irq,6,0);HAL_NVIC_EnableIRQ(irq);r->configured=true;
 }
 r->cursor.position=0;
 const HAL_StatusTypeDef result=HAL_UARTEx_ReceiveToIdle_DMA(u,r->data,CAP);
 r->discard=result!=HAL_OK;
 taskEXIT_CRITICAL();return result==HAL_OK;
}
/* Check both pending flags even if the UART IDLE IRQ arrives before DMA. */
static bool discard_overrun(Receiver *r) {
 if(r->discard)return true;
 if(!__HAL_DMA_GET_FLAG(&r->dma,__HAL_DMA_GET_HT_FLAG_INDEX(&r->dma)) ||
    !__HAL_DMA_GET_FLAG(&r->dma,__HAL_DMA_GET_TC_FLAG_INDEX(&r->dma)))return false;
 r->discard=true;
 r->uart->ErrorCode|=HAL_UART_ERROR_DMA;
 HAL_UART_ErrorCallback(r->uart);
 return true;
}
void AppUartDma_Irq(unsigned port) {
 if(port>=2 || !receivers[port].configured)return;
 Receiver *r=&receivers[port];
 (void)discard_overrun(r);
 HAL_DMA_IRQHandler(&r->dma);
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *u,uint16_t size) {
 (void)size;Receiver *r=find(u);if(!r||r->uart!=u||r->discard||u->ErrorCode!=HAL_UART_ERROR_NONE)return;
 const uint32_t remaining=__HAL_DMA_GET_COUNTER(&r->dma);
 if(remaining>CAP || discard_overrun(r))return;
 const uint16_t position=(uint16_t)((CAP-remaining)%CAP);
 RxSpan spans[2];int count=RxCursor_Advance(&r->cursor,position,CAP,spans);
 __DMB();const uint32_t now=HAL_GetTick();
 for(int i=0;i<count;i++) {
  if(u->Instance==USART2)AppVision_RxBytes(r->data+spans[i].offset,spans[i].length,now);
  else AppNet_RxBytes(r->data+spans[i].offset,spans[i].length,now);
 }
}
