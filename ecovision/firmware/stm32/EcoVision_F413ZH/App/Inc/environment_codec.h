#ifndef ENVIRONMENT_CODEC_H
#define ENVIRONMENT_CODEC_H
#include "app_aht20.h"
#include <stddef.h>
uint8_t Environment_Crc8(const uint8_t *data,size_t size);
int Environment_DecodeAht20(const uint8_t *data,size_t size,AHT20_SampleTypeDef *out);
uint16_t Environment_Adc12ToU16(uint16_t raw);
#endif
