#ifndef APP_BH1750_H
#define APP_BH1750_H
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
typedef enum { BH1750_OK=0, BH1750_PENDING=1, BH1750_IO_ERROR=-1,
               BH1750_SATURATED=-2 } Bh1750_Result;
typedef struct {
    bool attempted, configured;
    uint8_t address7;
    uint32_t last_action;
} Bh1750_Context;
typedef struct { uint16_t raw; uint32_t lux_x100; } Bh1750_Sample;
/* Zero-initialize context; called only by the environment task on I2C1.
 * No conversion delay; each HAL transaction has a 10 ms timeout. */
Bh1750_Result Bh1750_Poll(Bh1750_Context *ctx, I2C_HandleTypeDef *bus,
                          Bh1750_Sample *out);
#endif
