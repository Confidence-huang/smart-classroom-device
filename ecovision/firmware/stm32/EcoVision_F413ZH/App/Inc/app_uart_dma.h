#ifndef APP_UART_DMA_H
#define APP_UART_DMA_H
#include "main.h"
#include <stdbool.h>
bool AppUartDma_Start(UART_HandleTypeDef *);
void AppUartDma_Stop(UART_HandleTypeDef *);
void AppUartDma_Irq(unsigned port);
#endif
