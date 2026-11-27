#include "app_reed.h"
#include "stm32f4xx_hal.h"
#include "app_log.h"
#include "cmsis_os.h"

static AppReed_Snapshot latest;

void AppReed_GetSnapshot(AppReed_Snapshot *out)
{
    if (out == NULL) return;
    taskENTER_CRITICAL();
    *out = latest;
    taskEXIT_CRITICAL();
    if ((uint32_t)(HAL_GetTick() - out->sampled_at) >= 3000U)
        out->valid = false;
}

bool AppReed_Update(AppReed_Filter *filter, bool high, uint32_t now)
{
    if (!filter->started || high != filter->candidate_high)
    {
        filter->started = true;
        filter->candidate_high = high;
        filter->candidate_since = now;
        return false;
    }
    if ((uint32_t)(now - filter->candidate_since) >= 30U &&
        (!filter->valid || filter->stable_high != high))
    {
        filter->stable_high = high;
        filter->valid = true;
        return true;
    }
    return false;
}

void AppReed_Poll(void)
{
    static AppReed_Filter filter;
    static uint32_t last_bench_log_at;
    const bool high = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_13) == GPIO_PIN_SET;
    const uint32_t now = HAL_GetTick();
    const bool changed = AppReed_Update(&filter, high, now);
    taskENTER_CRITICAL();
    latest.valid = filter.valid;
    latest.do_high = filter.stable_high;
    latest.sampled_at = now;
    taskEXIT_CRITICAL();
    if (changed)
    {
        AppLog_Printf("[REED] stable_do=%u window=UNKNOWN calibration=PENDING\r\n",
                      high ? 1U : 0U);
    }
    if ((uint32_t)(now - last_bench_log_at) >= 1000U)
    {
        last_bench_log_at = now;
        AppLog_Printf("[REED_BENCH] raw_do=%u stable_valid=%u stable_do=%u\r\n",
                      high ? 1U : 0U,
                      filter.valid ? 1U : 0U,
                      filter.stable_high ? 1U : 0U);
    }
}
