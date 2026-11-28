#include "app_sgp30.h"
#include "cmsis_os.h"
#include "environment_codec.h"

enum {
    SGP30_HAL_ADDRESS = 0x58U << 1,
    SGP30_IO_TIMEOUT_MS = 10U,
    SGP30_WAIT_TICKS = 20U,
    SGP30_WARMUP_MS = 15000U,
    SGP30_RETRY_MS = 5000U,
    SGP30_MAX_INTERVAL_MS = 1500U
};

Sgp30_Result Sgp30_Poll(Sgp30_Context *ctx, I2C_HandleTypeDef *bus, Sgp30_Sample *out)
{
    const uint32_t now = HAL_GetTick();
    if (ctx->initialized && now - ctx->last_command_at > SGP30_MAX_INTERVAL_MS)
    {
        ctx->initialized = false;
        ctx->attempted = false;
    }
    if (!ctx->initialized && ctx->attempted && now - ctx->last_command_at < SGP30_RETRY_MS)
        return SGP30_IO_ERROR;

    uint8_t command[] = {0x20U, ctx->initialized ? 0x08U : 0x03U};
    ctx->last_command_at = now;
    ctx->attempted = true;
    int32_t lock = osKernelLock();
    HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit(bus, SGP30_HAL_ADDRESS, command,
                                                          sizeof command, SGP30_IO_TIMEOUT_MS);
    (void)osKernelRestoreLock(lock);
    if (tx_status != HAL_OK)
    {
        ctx->initialized = false;
        return SGP30_IO_ERROR;
    }
    if (!ctx->initialized)
    {
        ctx->started_at = HAL_GetTick();
        ctx->initialized = true;
        return SGP30_WARMING_UP;
    }

    /* The IAQ result needs up to 12 ms. Retry up to 2 times on transient CRC glitch. */
    uint8_t data[6];
    for (unsigned attempt = 0U; attempt < 3U; ++attempt)
    {
        if (attempt > 0U)
        {
            lock = osKernelLock();
            tx_status = HAL_I2C_Master_Transmit(bus, SGP30_HAL_ADDRESS, command,
                                                sizeof command, SGP30_IO_TIMEOUT_MS);
            (void)osKernelRestoreLock(lock);
            if (tx_status != HAL_OK)
            {
                ctx->initialized = false;
                return SGP30_IO_ERROR;
            }
        }
        if (osDelay(SGP30_WAIT_TICKS) != osOK)
        {
            ctx->initialized = false;
            return SGP30_IO_ERROR;
        }
        lock = osKernelLock();
        HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive(bus, SGP30_HAL_ADDRESS, data,
                                                             sizeof data, SGP30_IO_TIMEOUT_MS);
        (void)osKernelRestoreLock(lock);
        if (rx_status != HAL_OK)
        {
            ctx->initialized = false;
            return SGP30_IO_ERROR;
        }
        for (unsigned i = 0U; i < 6U; ++i) ctx->last_raw[i] = data[i];
        if (Environment_Crc8(data, 2U) == data[2] && Environment_Crc8(data + 3, 2U) == data[5])
            break;
        if (attempt == 2U) return SGP30_BAD_CRC;
    }
    if (now - ctx->started_at < SGP30_WARMUP_MS) return SGP30_WARMING_UP;

    const uint16_t eco2 = (uint16_t)((data[0] << 8) | data[1]);
    const uint16_t tvoc = (uint16_t)((data[3] << 8) | data[4]);
    if (eco2 < 400U || eco2 > 60000U || tvoc > 60000U) return SGP30_OUT_OF_RANGE;
    out->eco2_ppm = eco2;
    out->tvoc_ppb = tvoc;
    return SGP30_OK;
}
