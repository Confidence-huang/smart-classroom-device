#ifndef BH1750_HOST_HAL_H
#define BH1750_HOST_HAL_H
#include <stdint.h>
typedef struct { int unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *,uint16_t,uint32_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *,uint16_t,uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *,uint16_t,uint8_t *,uint16_t,uint32_t);
#endif
