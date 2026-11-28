#ifndef APP_ENVIRONMENT_H
#define APP_ENVIRONMENT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool th_valid,light_valid;
    int32_t temp_x100,humi_x100;
    uint16_t light_raw12,light_raw16;
    uint32_t sampled_at;
    int sensor_error;
    bool lux_valid;
    uint16_t bh1750_raw;
    uint32_t lux_x100;
    int light_error;
    bool air_valid;
    uint16_t eco2_ppm, tvoc_ppb;
    uint32_t air_sampled_at;
    int air_status;
} EnvironmentSnapshot;
void AppEnvironment_Run(void *argument);
void AppEnvironment_GetSnapshot(EnvironmentSnapshot *out);
#endif
