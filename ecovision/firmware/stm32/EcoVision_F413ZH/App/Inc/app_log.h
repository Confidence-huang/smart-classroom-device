/**
 * @file    app_log.h
 * @brief   串口日志输出（USART3 = 板载 ST-LINK 虚拟串口 VCP，115200 8N1）
 *
 * 用法：AppLog_Printf("temp=%ld.%02ld C\r\n", v / 100, v % 100);
 *
 * 说明：
 *  - 不使用 printf 重定向（那需要改编译器/MicroLIB 配置），改用 vsnprintf + HAL_UART_Transmit，
 *    在 Keil(ARMCC/AC6) 与 GCC 下行为一致，换工具链不用改代码。
 *  - 只支持整数格式（%d/%u/%ld/%lu/%s/%x），不要用 %f：newlib-nano 默认不带浮点格式化，
 *    Keil 也要额外勾选支持；本工程所有量都用“放大 100 倍的整数”表示。
 */
#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdint.h>

/**
 * @brief  初始化日志（当前只校验串口句柄，可留空调用）
 */
void AppLog_Init(void);

/**
 * @brief  格式化并阻塞发送到 USART3（VCP）
 * @param  fmt 格式串（只支持整数格式）
 * @note   一帧最长 255 字节，超长会被截断；阻塞时间上界约 200 ms（1 帧 @115200 约 22 ms）
 */
void AppLog_Printf(const char *fmt, ...);

/**
 * @brief  以十六进制打印一段内存，用于调试 I2C/串口原始数据
 */
void AppLog_Hex(const char *tag, const uint8_t *data, uint16_t len);

#endif /* APP_LOG_H */
