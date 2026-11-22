/**
 * @file    app_log.c
 * @brief   串口日志实现：USART3 / VCP，115200 8N1
 *
 * 依赖：CubeMX 生成的 huart3（PD8=TX、PD9=RX，经 SB5/SB6 接板载 ST-LINK，Windows 上是 COM 口）
 */
#include "main.h"
#include "app_log.h"
#include "cmsis_os.h"

#include <stdarg.h>
#include <stdio.h>

/* CubeMX 在 main.c 中定义，这里只引用 */
extern UART_HandleTypeDef huart3;

#define APP_LOG_BUF_SIZE   256U
#define APP_LOG_TX_TIMEOUT 200U   /* ms */

static char s_log_buf[APP_LOG_BUF_SIZE];
static osMutexId_t log_mutex;

void AppLog_Init(void)
{
    log_mutex=osMutexNew(NULL);
    if(log_mutex==NULL) Error_Handler();
    /* huart3 已由 MX_USART3_UART_Init() 初始化，这里无需额外动作。
     * 如需检查串口是否就绪，可在这里读取 huart3.Instance 等字段。 */
}

void AppLog_Printf(const char *fmt, ...)
{
    va_list ap;
    int len;

    if (fmt == NULL)
    {
        return;
    }

    if(log_mutex && osMutexAcquire(log_mutex,osWaitForever)!=osOK) return;
    va_start(ap, fmt);
    len = vsnprintf(s_log_buf, sizeof(s_log_buf), fmt, ap);
    va_end(ap);

    if (len <= 0)
    {
        if(log_mutex) (void)osMutexRelease(log_mutex);
        return;
    }
    if (len > (int)sizeof(s_log_buf) - 1)
    {
        len = (int)sizeof(s_log_buf) - 1;   /* 超长截断 */
    }

    (void)HAL_UART_Transmit(&huart3, (uint8_t *)s_log_buf, (uint16_t)len, APP_LOG_TX_TIMEOUT);
    if(log_mutex) (void)osMutexRelease(log_mutex);
}

void AppLog_Hex(const char *tag, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    AppLog_Printf("%s[%u]:", (tag != NULL) ? tag : "hex", (unsigned)len);
    for (uint16_t i = 0U; i < len; i++)
    {
        AppLog_Printf(" %02X", (unsigned)data[i]);
    }
    AppLog_Printf("\r\n");
}
