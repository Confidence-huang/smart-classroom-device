#include "environment_codec.h"
uint8_t Environment_Crc8(const uint8_t *data,size_t size) {
    uint8_t crc=0xff;
    for(size_t i=0;i<size;i++) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;bit++) crc=(uint8_t)((crc&0x80U)?(crc<<1)^0x31U:crc<<1);
    }
    return crc;
}
int Environment_DecodeAht20(const uint8_t *data,size_t size,AHT20_SampleTypeDef *out) {
    if(!data || !out || size!=7) return AHT20_ERR_NACK;
    if(data[0]&0x80) return AHT20_ERR_BUSY;
    if(!(data[0]&0x08)) return AHT20_ERR_CAL;
    if(Environment_Crc8(data,6)!=data[6]) return AHT20_ERR_CRC;
    uint32_t h=((uint32_t)data[1]<<12)|((uint32_t)data[2]<<4)|(data[3]>>4);
    uint32_t t=(((uint32_t)data[3]&15U)<<16)|((uint32_t)data[4]<<8)|data[5];
    int32_t temperature=(int32_t)(((uint64_t)t*20000U)>>20)-5000;
    int32_t humidity=(int32_t)(((uint64_t)h*10000U)>>20);
    if(temperature < -4000 || temperature > 8500 || humidity<0 || humidity>10000) return AHT20_ERR_RANGE;
    out->temp_x100=temperature;out->humi_x100=humidity;
    return AHT20_OK;
}
uint16_t Environment_Adc12ToU16(uint16_t raw) {
    raw&=0x0fff;
    return (uint16_t)((raw<<4)|(raw>>8));
}
