#include "environment_codec.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    AHT20_SampleTypeDef s;
    uint8_t packet[7]={0x18,0x80,0,0x08,0,0,0};
    packet[6]=Environment_Crc8(packet,6);
    assert(Environment_Crc8((const uint8_t *)"123456789",9)==0xf7);
    assert(Environment_DecodeAht20(packet,7,&s)==AHT20_OK);
    assert(s.humi_x100==5000 && s.temp_x100==5000);
    packet[1]^=1;
    assert(Environment_DecodeAht20(packet,7,&s)==AHT20_ERR_CRC);
    assert(Environment_DecodeAht20(packet,6,&s)!=AHT20_OK);
    packet[0]=0x98;
    assert(Environment_DecodeAht20(packet,7,&s)==AHT20_ERR_BUSY);
    packet[0]=0;
    assert(Environment_DecodeAht20(packet,7,&s)==AHT20_ERR_CAL);
    assert(Environment_Adc12ToU16(0)==0);
    assert(Environment_Adc12ToU16(4095)==65535);
    assert(Environment_Adc12ToU16(2048)==32776);
    puts("PASS: AHT20 CRC8, decode, malformed/CRC/busy/calibration rejection; ADC12 to ADC16 endpoints");
    return 0;
}
