#ifndef APP_SGP30_H
#define APP_SGP30_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SGP30_OK = 0,
    SGP30_WARMING_UP = 1,
    SGP30_IO_ERROR = -1,
    SGP30_BAD_CRC = -2,
    SGP30_OUT_OF_RANGE = -3
} Sgp30_Result;

typedef struct {
    uint32_t started_at, last_command_at;
    bool attempted, initialized;
    uint8_t last_raw[6];
} Sgp30_Context;

typedef struct {
    uint16_t eco2_ppm, tvoc_ppb;
} Sgp30_Sample;

/* Zero-initialize the context. Poll every second from the shared I2C owner task.
 * The sample is written only on SGP30_OK. */
Sgp30_Result Sgp30_Poll(Sgp30_Context *ctx, I2C_HandleTypeDef *bus, Sgp30_Sample *out);

/* Compute Sensirion 8.8 fixed-point absolute humidity (g/m^3) from temp_x100 (C*100) and rh_x100 (%*100). */
uint16_t Sgp30_AbsoluteHumidity88(int32_t temp_x100, int32_t rh_x100);

/* Send SGP30 set_humidity (0x2061) command using live AHT20 temperature & relative humidity. */
bool Sgp30_SetHumidity(const Sgp30_Context *ctx, I2C_HandleTypeDef *bus, int32_t temp_x100, int32_t rh_x100);

#endif
