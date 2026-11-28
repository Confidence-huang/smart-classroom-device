#include "app_sgp30.h"
#include "cmsis_os.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t clock_ms;
static unsigned commands, reads;
static uint16_t last_cmd;
static bool tx_failed, rx_failed;
static uint8_t reply[] = {0x01U, 0x90U, 0x4cU, 0x00U, 0x00U, 0x81U};

uint32_t HAL_GetTick(void) { return clock_ms; }

int32_t osDelay(uint32_t ticks)
{
    assert(ticks >= 13U && ticks <= 20U);
    clock_ms += ticks;
    return 0;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus, uint16_t addr,
    uint8_t *data, uint16_t length, uint32_t timeout)
{
    assert(bus && addr == (0x58U << 1) && length == 2U && timeout <= 10U);
    last_cmd = (uint16_t)((data[0] << 8) | data[1]);
    ++commands;
    return tx_failed ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *bus, uint16_t addr,
    uint8_t *data, uint16_t length, uint32_t timeout)
{
    assert(bus && addr == (0x58U << 1) && length == sizeof reply && timeout <= 10U);
    ++reads;
    if (rx_failed) return HAL_ERROR;
    memcpy(data, reply, sizeof reply);
    return HAL_OK;
}

int main(void)
{
    I2C_HandleTypeDef bus = {0};
    Sgp30_Context ctx = {0};
    Sgp30_Sample sample = {0};

    clock_ms = UINT32_MAX - 4000U;
    uint32_t start = clock_ms;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_WARMING_UP);
    assert(last_cmd == 0x2003U && reads == 0U);

    for (uint32_t second = 1; second < 15U; ++second) {
        clock_ms = start + second * 1000U;
        assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_WARMING_UP);
        assert(last_cmd == 0x2008U);
    }
    clock_ms = start + 15000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_OK);
    assert(sample.eco2_ppm == 400U && sample.tvoc_ppb == 0U && reads == 15U);

    const uint8_t measured[] = {0x02U, 0x58U, 0x9fU, 0x01U, 0x2cU, 0x8eU};
    memcpy(reply, measured, sizeof reply);
    reply[5] ^= 1U;
    clock_ms = start + 16000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_BAD_CRC);
    reply[5] ^= 1U;
    reply[2] ^= 1U;
    clock_ms = start + 17000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_BAD_CRC);
    reply[2] ^= 1U;
    clock_ms = start + 18000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_OK);
    assert(sample.eco2_ppm == 600U && sample.tvoc_ppb == 300U);

    reply[0] = reply[1] = 0xffU;
    reply[2] = 0xacU;
    clock_ms = start + 19000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_OUT_OF_RANGE);
    assert(sample.eco2_ppm == 600U && sample.tvoc_ppb == 300U);

    rx_failed = true;
    clock_ms = start + 20000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_IO_ERROR);
    unsigned before = commands;
    clock_ms = start + 21000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_IO_ERROR);
    assert(commands == before);
    rx_failed = false;
    clock_ms = start + 26000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_WARMING_UP);
    assert(last_cmd == 0x2003U);
    clock_ms = start + 28000U;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_WARMING_UP);
    assert(last_cmd == 0x2003U);

    ctx = (Sgp30_Context){0};
    tx_failed = true;
    assert(Sgp30_Poll(&ctx, &bus, &sample) == SGP30_IO_ERROR);
    puts("PASS: SGP30 startup, 1 s measurements, warm-up, CRC, range, retry and tick wrap");
    return 0;
}
