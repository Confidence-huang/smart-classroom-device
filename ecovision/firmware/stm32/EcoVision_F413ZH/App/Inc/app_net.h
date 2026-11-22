#ifndef APP_NET_H
#define APP_NET_H
#include "main.h"
void AppNet_Run(void *argument);
void AppNet_RxComplete(UART_HandleTypeDef *uart);
void AppNet_RxError(UART_HandleTypeDef *uart);
const char *AppNet_State(void);
#endif
