/* Only this adapter owns UART2/6 RX DMA. No allocation, parsing or logs in IRQ.
 * F413 SRAM is DMA-accessible, no D-cache. HT/TC/IDLE IRQs all priority 6.
 * Service latency must stay below 22 ms (256 bytes at 115200 8N1). */
#include "app_uart_dma.h"
#include "rx_cursor.h"
#include "cmsis_os.h"
enum { CAP=512 };
typedef struct { DMA_HandleTypeDef dma;UART_HandleTypeDef *uart;RxCursor cursor;uint8_t data[CAP];bool configured,discard; } Receiver;
static Receiver receivers[2];
static Receiver *find(UART_HandleTypeDef *u) {
 if(u->Instance==USART2)return &receivers[0];
 if(u->Instance==USART6)return &receivers[1];
 return NULL;
}
void AppUartDma_Stop(UART_HandleTypeDef *u) {
 if(find(u)) (void)HAL_UART_AbortReceive(u);
}
bool AppUartDma_Start(UART_HandleTypeDef *u) {
 Receiver *r=find(u);if(!r)return false;
 /* Called from a task. Serialize cursor reset and ring reset with both IRQs. */
 taskENTER_CRITICAL();
 if(!r->configured) {
  __HAL_RCC_DMA1_CLK_ENABLE();__HAL_RCC_DMA2_CLK_ENABLE();
  r->dma.Instance=u->Instance==USART2?DMA1_Stream5:DMA2_Stream1;
  r->dma.Init.Channel=u->Instance==USART2?DMA_CHANNEL_4:DMA_CHANNEL_5;
  r->dma.Init.Direction=DMA_PERIPH_TO_MEMORY;r->dma.Init.PeriphInc=DMA_PINC_DISABLE;
  r->dma.Init.MemInc=DMA_MINC_ENABLE;r->dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
  r->dma.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;r->dma.Init.Mode=DMA_CIRCULAR;
  r->dma.Init.Priority=DMA_PRIORITY_HIGH;r->dma.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
  if(HAL_DMA_Init(&r->dma)!=HAL_OK){taskEXIT_CRITICAL();return false;}
  IRQn_Type irq=u->Instance==USART2?DMA1_Stream5_IRQn:DMA2_Stream1_IRQn;
  HAL_NVIC_SetPriority(irq,6,0);HAL_NVIC_EnableIRQ(irq);r->configured=true;
 }
 r->uart=u;__HAL_LINKDMA(u,hdmarx,r->dma);
 (void)HAL_UART_AbortReceive(u);
 /* CubeMX MSP deinit may reset the handle during recovery. Always re-init. */
 if(HAL_DMA_Init(&r->dma)!=HAL_OK){taskEXIT_CRITICAL();return false;}
 r->cursor.position=0;r->discard=false;
 HAL_StatusTypeDef result=HAL_UARTEx_ReceiveToIdle_DMA(u,r->data,CAP);
 taskEXIT_CRITICAL();return result==HAL_OK;
}
void AppUartDma_Irq(unsigned port) {
 if(port>=2 || !receivers[port].configured)return;
 Receiver *r=&receivers[port];
 /* Both boundaries pending: service latency is no longer provably safe.
  * Discard conservatively instead of accepting overwritten bytes as fresh. */
 if(__HAL_DMA_GET_FLAG(&r->dma,__HAL_DMA_GET_HT_FLAG_INDEX(&r->dma)) &&
    __HAL_DMA_GET_FLAG(&r->dma,__HAL_DMA_GET_TC_FLAG_INDEX(&r->dma))) {
  r->discard=true;r->uart->ErrorCode|=HAL_UART_ERROR_DMA;
  HAL_UART_ErrorCallback(r->uart);
 }
 HAL_DMA_IRQHandler(&r->dma);
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *u,uint16_t size) {
 (void)size;Receiver *r=find(u);if(!r||r->uart!=u||r->discard)return;
 const uint32_t remaining=__HAL_DMA_GET_COUNTER(&r->dma);
 if(remaining>CAP)return;
 const uint16_t position=(uint16_t)((CAP-remaining)%CAP);
 RxSpan spans[2];int count=RxCursor_Advance(&r->cursor,position,CAP,spans);
 __DMB();const uint32_t now=HAL_GetTick();
 for(int i=0;i<count;i++) {
  if(u->Instance==USART2)AppVision_RxBytes(r->data+spans[i].offset,spans[i].length,now);
  else AppNet_RxBytes(r->data+spans[i].offset,spans[i].length,now);
 }
}
