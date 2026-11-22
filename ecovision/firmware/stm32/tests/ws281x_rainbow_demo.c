/* Isolated WS281x demo for the NUCLEO-F413ZH bench image. */
#include "main.h"
#include "app_log.h"
#include "cmsis_os.h"
#include "ws281x_rainbow.h"

bool Ws281xBench_Init(void);
bool Ws281xBench_Show(bool first_green);
bool Ws281xBench_Write(const uint8_t frame[RAINBOW_PIXELS][3]);
void Ws281xRainbow_Run(void);

void Ws281xRainbow_Run(void)
{
    if (!Ws281xBench_Init() || !Ws281xBench_Show(false))
    {
        AppLog_Printf("[LED] WS281x initialization failed\r\n");
        return;
    }

    uint8_t frame[RAINBOW_PIXELS][3];
    unsigned head = 0U;
    uint8_t hue = 0U;
    bool reverse = false;
    RainbowButton button = RainbowButton_Init(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);
    const uint32_t start = HAL_GetTick();
    uint32_t frame_at = start - 50U;
    uint32_t heartbeat_at = start;
    AppLog_Printf("[LED] Rainbow ready: B1 reverses direction\r\n");

    for (;;)
    {
        const uint32_t now = HAL_GetTick();
        const bool level = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET;
        if (RainbowButton_Pressed(&button, level, now))
        {
            reverse = !reverse;
            AppLog_Printf("[LED] direction=%s\r\n", reverse ? "reverse" : "forward");
        }

        if (now - frame_at >= 50U)
        {
            Rainbow_Render(frame, head, hue, reverse);
            if (!Ws281xBench_Write(frame))
            {
                AppLog_Printf("[LED] WS281x DMA transfer failed\r\n");
                return;
            }
            frame_at = now;
            head = (head + (reverse ? RAINBOW_PIXELS - 1U : 1U)) % RAINBOW_PIXELS;
            hue = (uint8_t)(hue + 5U);
        }

        if (now - heartbeat_at >= 500U)
        {
            heartbeat_at = now;
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        }
        osDelay(10U);
    }
}
