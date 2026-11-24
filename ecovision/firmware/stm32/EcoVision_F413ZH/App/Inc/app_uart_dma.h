#ifndef APP_UART_DMA_H
#define APP_UART_DMA_H
#include "main.h"
#include <stdbool.h>
bool AppUartDma_Start(UART_HandleTypeDef *);
void AppUartDma_Stop(UART_HandleTypeDef *);
void AppUartDma_Irq(unsigned port);
void AppVision_RxBytes(const uint8_t *,uint16_t,uint32_t);
void AppNet_RxBytes(const uint8_t *,uint16_t,uint32_t);
#endif
