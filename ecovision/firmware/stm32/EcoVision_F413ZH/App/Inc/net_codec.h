#ifndef NET_CODEC_H
#define NET_CODEC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {NET_AT_NONE,NET_AT_LINE,NET_AT_PROMPT,NET_AT_DROPPED} NetAtEvent;
typedef struct {char line[512];size_t used;bool dropping;} NetAtParser;
typedef struct {
 char host[128],user[128],password[128],device[33];
 uint16_t port,ca_length;char ca[4097],legacy_topic[128],room[33];
} NetConfig;
typedef struct {
 uint32_t uptime,seq,vision_age,temp_age,utc;
 bool vision_valid,temp_valid,have_vision,have_temp,time_valid;
 uint8_t count,fan,led;int32_t temp_x100,rh_x100;
 bool light_valid;uint16_t light_raw16;
 uint8_t occupancy;bool local_off;uint32_t control_epoch;
} NetSample;
uint32_t Net_Crc32(const uint8_t *data,size_t size);
NetAtEvent NetAt_Feed(NetAtParser *p,uint8_t byte);
bool Net_Result(const char *line,const char *prefix,int *values,size_t count);
bool Net_ConfigDecode(const uint8_t *bytes,size_t size,NetConfig *out);
bool Net_ClockUTC(const char *line,uint32_t *utc);
int Net_IPv4Profile(char *out,size_t size,const char *line);
int Net_Telemetry(char *out,size_t size,const char *device,const char *boot,const NetSample *s);
int Net_LegacyTelemetry(char *out,size_t size,const char *device,const char *room,const NetSample *s);
#endif
