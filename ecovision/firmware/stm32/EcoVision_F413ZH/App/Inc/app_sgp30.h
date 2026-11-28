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

#endif
