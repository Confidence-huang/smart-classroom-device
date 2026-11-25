#ifndef MOCK_DMA_MAIN_H
#define MOCK_DMA_MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef int HAL_StatusTypeDef;enum {HAL_OK,HAL_ERROR};typedef int IRQn_Type;
typedef struct {uint32_t count,flags;} Stream;
typedef struct {uint32_t Channel,Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment,Mode,Priority,FIFOMode;} DmaInit;
typedef struct {Stream *Instance;DmaInit Init;void *Parent;} DMA_HandleTypeDef;
typedef struct {void *Instance;uint32_t ErrorCode;DMA_HandleTypeDef *hdmarx;uint8_t *buffer;uint16_t capacity;} UART_HandleTypeDef;
extern Stream stream1,stream5;
#define USART2 ((void *)2)
#define USART6 ((void *)6)
#define DMA1_Stream5 (&stream5)
#define DMA2_Stream1 (&stream1)
#define DMA1_Stream5_IRQn 15
#define DMA2_Stream1_IRQn 21
#define DMA_CHANNEL_4 4
#define DMA_CHANNEL_5 5
#define DMA_PERIPH_TO_MEMORY 0
#define DMA_PINC_DISABLE 0
#define DMA_MINC_ENABLE 1
#define DMA_PDATAALIGN_BYTE 0
#define DMA_MDATAALIGN_BYTE 0
#define DMA_CIRCULAR 1
#define DMA_PRIORITY_HIGH 2
#define DMA_FIFOMODE_DISABLE 0
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMA2_CLK_ENABLE() ((void)0)
#define __HAL_LINKDMA(u,member,d) do {(u)->member=&(d);(d).Parent=(u);}while(0)
#define __HAL_DMA_GET_COUNTER(d) ((d)->Instance->count)
#define HAL_UART_ERROR_NONE 0U
#define HAL_UART_ERROR_DMA 16U
#define __HAL_DMA_GET_FLAG(d,f) ((d)->Instance->flags&(f))
#define __HAL_DMA_GET_HT_FLAG_INDEX(d) 1U
#define __HAL_DMA_GET_TC_FLAG_INDEX(d) 2U
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
#define __DMB() ((void)0)
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *,uint8_t *,uint16_t);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *);
void HAL_NVIC_SetPriority(IRQn_Type,unsigned,unsigned);
void HAL_NVIC_EnableIRQ(IRQn_Type);
uint32_t HAL_GetTick(void);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *,uint16_t);
#endif
