#ifndef APP_NET_H
#define APP_NET_H
#include "main.h"
void AppNet_Run(void *argument);
void AppNet_RxBytes(const uint8_t *data, uint16_t length, uint32_t at);
void AppNet_RxError(UART_HandleTypeDef *uart);
const char *AppNet_State(void);
#endif
