#include "net_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(Net_Crc32((const uint8_t *)"123456789",9)==0xcbf43926U);
    NetAtParser p={0}; unsigned lines=0,prompts=0;
    const char *input="\r\nOK\r\n+QMTCONN: 0,0,0\r\n> ";
    for(const char *s=input;*s;s++) {
        NetAtEvent event=NetAt_Feed(&p,(uint8_t)*s);
        if(event==NET_AT_LINE) lines++;
        if(event==NET_AT_PROMPT) prompts++;
    }
    assert(lines==2 && prompts==1);
    int nums[3];
    assert(Net_Result("+QMTPUBEX: 0,19,0","+QMTPUBEX:",nums,3));
    assert(nums[0]==0 && nums[1]==19 && nums[2]==0);
    assert(!Net_Result("+QMTPUBEX: 0,19,0junk","+QMTPUBEX:",nums,3));
    assert(!Net_Result("+QMTPUBEX: 0,19","+QMTPUBEX:",nums,3));
    assert(!Net_Result("+QMTPUBEX: 0,99999999999999999999999,0","+QMTPUBEX:",nums,3));
    uint32_t utc;
    assert(Net_ClockUTC("+QLTS: \"2026/11/21,22:46:55+32,0\"",&utc));
    uint32_t same;
    assert(Net_ClockUTC("+CCLK: \"26/11/21,14:46:55+00\"",&same) && same==utc);
    assert(!Net_ClockUTC("+CCLK: \"26/02/31,14:46:55+00\"",&same));
    assert(!Net_ClockUTC("+CCLK: \"26/11/21,24:46:55+00\"",&same));
    char profile[384];
    assert(Net_IPv4Profile(profile,sizeof profile,"+QICSGP: 3,\"carrier\",\"\",\"\",0")>0);
    assert(!strcmp(profile,"AT+QICSGP=1,1,\"carrier\",\"\",\"\",0"));
    assert(Net_IPv4Profile(profile,sizeof profile,"+QICSGP: 3,\"carrier\",\"\",\"\",0\rAT")<0);
    assert(Net_IPv4Profile(profile,8,"+QICSGP: 3,\"\",\"\",\"\",0")<0);
    memset(&p,0,sizeof p);
    for(unsigned i=0;i<600;i++) (void)NetAt_Feed(&p,'A');
    assert(NetAt_Feed(&p,'\n')==NET_AT_DROPPED);
    assert(NetAt_Feed(&p,'O')==NET_AT_NONE);
    assert(NetAt_Feed(&p,'K')==NET_AT_NONE);
    assert(NetAt_Feed(&p,'\n')==NET_AT_LINE && !strcmp(p.line,"OK"));
    NetSample s={0}; s.uptime=1000;s.seq=5;
    char json[1200];
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)>0);
    assert(strstr(json,"\"count\":null") && strstr(json,"\"temp_c\":null"));
    assert(strstr(json,"\"time_valid\":false") && strstr(json,"VISION_FAULT"));
    s.vision_valid=true;s.count=0;s.vision_age=0;s.temp_valid=true;s.temp_x100=-123;s.rh_x100=4500;s.temp_age=0;
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)>0);
    assert(strstr(json,"\"count\":0") && strstr(json,"\"temp_c\":-1.23"));
    assert(!strstr(json,"VISION_FAULT") && !strstr(json,"SENSOR_FAULT"));
    assert(Net_Telemetry(json,8,"ev001","0123456789abcdef",&s)<0);
    assert(Net_Telemetry(json,sizeof json,"bad\"id","0123456789abcdef",&s)<0);
    assert(!Net_ConfigDecode((const uint8_t *)"garbage",7,0));
    uint8_t fixture[1024]={0};NetConfig decoded;
    memcpy(fixture,"EVNET01",8);
    strcpy((char *)fixture+16,"broker.example.invalid");strcpy((char *)fixture+144,"test-user");
    strcpy((char *)fixture+272,"test-only-placeholder");strcpy((char *)fixture+400,"ev001");
    fixture[433]=0xb3;fixture[434]=0x22;fixture[435]=28;
    memcpy(fixture+437,"-----BEGIN CERTIFICATE-----\n",28);
    uint32_t length=449;memcpy(fixture+8,&length,4);
    uint32_t crc=Net_Crc32(fixture+16,length);memcpy(fixture+12,&crc,4);
    assert(Net_ConfigDecode(fixture,465,&decoded) && decoded.port==8883 && decoded.legacy_topic[0]==0);
    strcpy((char *)fixture+465,"classroom/room01/telemetry");strcpy((char *)fixture+593,"room01");
    length+=161;memcpy(fixture+8,&length,4);crc=Net_Crc32(fixture+16,length);memcpy(fixture+12,&crc,4);
    assert(Net_ConfigDecode(fixture,626,&decoded) && !strcmp(decoded.room,"room01"));
    fixture[280]^=1;
    assert(!Net_ConfigDecode(fixture,626,&decoded));
    s.time_valid=true;s.utc=1795272586U;s.light_valid=true;s.light_raw16=12000;
    assert(Net_LegacyTelemetry(json,sizeof json,"ev001","room01",&s)>0);
    assert(strstr(json,"\"simulated\":false") && strstr(json,"\"people_count\":0"));
    assert(strstr(json,"2026-11-21T14:49:46Z") && strstr(json,"\"light_lux\":null"));
    s.temp_valid=false;
    assert(Net_LegacyTelemetry(json,sizeof json,"ev001","room01",&s)<0);
    s.temp_valid=true;s.occupancy=1;s.control_epoch=42;
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)>0);
    assert(strstr(json,"\"occupancy\":\"OCCUPIED\"") && strstr(json,"\"control_epoch\":42"));
    s.local_off=true;s.fan=0;s.led=0;
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)>0);
    assert(strstr(json,"\"mode\":\"LOCAL_OFF\""));
    s.fan=1;
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)<0);
    s.fan=0;
    s.occupancy=3;
    assert(Net_Telemetry(json,sizeof json,"ev001","0123456789abcdef",&s)<0);
    puts("PASS: AT framing/overflow, URC bounds, UTC/timezone/calendar, APN preservation, private config CRC/bounds, formal validity and legacy compatibility");
    return 0;
}
