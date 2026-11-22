#include "app_bh1750.h"
#include <stddef.h>

Bh1750_Result Bh1750_Poll(Bh1750_Context *ctx, I2C_HandleTypeDef *bus,
                          Bh1750_Sample *out)
{
    if (ctx == NULL || bus == NULL || out == NULL) return BH1750_IO_ERROR;
    const uint32_t now = HAL_GetTick();
    if (!ctx->configured)
    {
        if (ctx->attempted && (uint32_t)(now - ctx->last_action) < 2000U)
            return BH1750_PENDING;
        ctx->attempted = true;
        ctx->last_action = now;
        ctx->address7 = 0U;
        const uint8_t addresses[] = {0x23U, 0x5cU};
        for (unsigned i = 0U; i < 2U; ++i)
        {
            if (HAL_I2C_IsDeviceReady(bus, (uint16_t)(addresses[i] << 1), 1U, 10U) == HAL_OK)
            { ctx->address7 = addresses[i]; break; }
        }
        if (ctx->address7 == 0U) return BH1750_IO_ERROR;
        /* Power on, clear data, explicitly set MTreg=69, continuous H-res. */
        const uint8_t commands[] = {0x01U, 0x07U, 0x42U, 0x65U, 0x10U};
        for (unsigned i = 0U; i < sizeof commands; ++i)
        {
            uint8_t command = commands[i];
            if (HAL_I2C_Master_Transmit(bus, (uint16_t)(ctx->address7 << 1),
                                       &command, 1U, 10U) != HAL_OK)
                return BH1750_IO_ERROR;
        }
        ctx->last_action = HAL_GetTick();
        ctx->configured = true;
        return BH1750_PENDING;
    }
    if ((uint32_t)(now - ctx->last_action) < 180U) return BH1750_PENDING;
    uint8_t data[2];
    if (HAL_I2C_Master_Receive(bus, (uint16_t)(ctx->address7 << 1), data, 2U, 10U) != HAL_OK)
    {
        ctx->configured = false;
        ctx->last_action = HAL_GetTick();
        return BH1750_IO_ERROR;
    }
    ctx->last_action = HAL_GetTick();
    out->raw = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    out->lux_x100 = ((uint32_t)out->raw * 1000U + 6U) / 12U;
    return out->raw == UINT16_MAX ? BH1750_SATURATED : BH1750_OK;
}
