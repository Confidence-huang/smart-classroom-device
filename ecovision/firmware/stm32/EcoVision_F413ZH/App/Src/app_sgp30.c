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

uint16_t Sgp30_AbsoluteHumidity88(int32_t temp_x100, int32_t rh_x100)
{
    /* Saturation vapor pressure es(T) in Pa from -10 C to +60 C in 5 C steps. */
    static const uint32_t es_pa[15] = {
        286U, 421U, 611U, 872U, 1228U, 1705U, 2338U, 3169U,
        4245U, 5626U, 7381U, 9589U, 12344U, 15752U, 19933U
    };
    if (rh_x100 <= 0) return 0U;
    if (rh_x100 > 10000) rh_x100 = 10000;
    if (temp_x100 < -1000) temp_x100 = -1000;
    if (temp_x100 > 6000) temp_x100 = 6000;

    uint32_t offset = (uint32_t)(temp_x100 + 1000);
    unsigned idx = (unsigned)(offset / 500U);
    uint32_t frac = offset % 500U;
    uint32_t es = (idx >= 14U) ? es_pa[14]
        : es_pa[idx] + ((es_pa[idx + 1U] - es_pa[idx]) * frac) / 500U;

    /* AH(g/m^3) * 256 = 216.7 * (RH/100) * (es/100) / (273.15 + T) * 256 */
    uint64_t denom = (uint64_t)(27315 + temp_x100) * 100000ULL;
    uint64_t ah_x256 = (2167ULL * (uint64_t)es * (uint64_t)rh_x100 * 256ULL) / denom;
    if (ah_x256 < 1ULL) return 1U;
    if (ah_x256 > 0xFFFFULL) return 0xFFFFU;
    return (uint16_t)ah_x256;
}

bool Sgp30_SetHumidity(const Sgp30_Context *ctx, I2C_HandleTypeDef *bus, int32_t temp_x100, int32_t rh_x100)
{
    if (!ctx || !bus || !ctx->initialized) return false;
    uint16_t ah = Sgp30_AbsoluteHumidity88(temp_x100, rh_x100);
    if (ah == 0U) return false;
    uint8_t frame[5];
    frame[0] = 0x20U;
    frame[1] = 0x61U;
    frame[2] = (uint8_t)(ah >> 8);
    frame[3] = (uint8_t)(ah & 0xFFU);
    frame[4] = Environment_Crc8(frame + 2, 2U);
    int32_t lock = osKernelLock();
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(bus, SGP30_HAL_ADDRESS, frame,
                                                   sizeof frame, SGP30_IO_TIMEOUT_MS);
    (void)osKernelRestoreLock(lock);
    return st == HAL_OK;
}
