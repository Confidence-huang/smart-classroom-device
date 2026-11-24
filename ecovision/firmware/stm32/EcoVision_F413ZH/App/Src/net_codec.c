#include "net_codec.h"
#include "control_policy.h" /* Telemetry reports the same applied level range as commands. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

uint32_t Net_Crc32(const uint8_t *data,size_t size) {
 uint32_t crc=0xffffffffU;
 for(size_t i=0;i<size;i++){crc^=data[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((crc&1U)?0xedb88320U:0U);}
 return ~crc;
}
NetAtEvent NetAt_Feed(NetAtParser *p,uint8_t byte) {
 if(byte=='\r')return NET_AT_NONE;
 if(byte=='\n') {
  if(p->dropping){p->dropping=false;p->used=0;return NET_AT_DROPPED;}
  if(!p->used)return NET_AT_NONE;
  p->line[p->used]=0;p->used=0;return NET_AT_LINE;
 }
 if(p->dropping)return NET_AT_NONE;
 if(byte=='>' && !p->used)return NET_AT_PROMPT;
 if(!p->used && byte==' ')return NET_AT_NONE;
 if(byte<32 || byte>126 || p->used>=sizeof p->line-1){p->dropping=true;p->used=0;return NET_AT_NONE;}
 p->line[p->used++]=(char)byte;return NET_AT_NONE;
}
bool Net_Result(const char *line,const char *prefix,int *values,size_t count) {
 size_t n=strlen(prefix);if(strncmp(line,prefix,n))return false;
 const char *p=line+n;
 for(size_t i=0;i<count;i++) {
  while(*p==' ')p++;
  if(!((*p>='0'&&*p<='9')||*p=='-'))return false;
  char *end;errno=0;long value=strtol(p,&end,10);
  if(errno==ERANGE || end==p || value<INT_MIN || value>INT_MAX)return false;
  values[i]=(int)value;p=end;while(*p==' ')p++;
  if(i+1<count){if(*p++!=',')return false;}else if(*p)return false;
 }
 return true;
}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool safe_at(const char *s,size_t size) {
 size_t n=0;while(n<size && s[n]){if((unsigned char)s[n]<32 || (unsigned char)s[n]>126 || s[n]=='"' || s[n]=='\\')return false;n++;}
 return n>0 && n<size;
}
static bool identifier(const char *s,size_t max) {
 size_t n=0;for(;s[n];n++){char c=s[n];if(n>=max || !((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;}
 return n>0 && ((s[0]>='a'&&s[0]<='z')||(s[0]>='0'&&s[0]<='9'));
}
bool Net_ConfigDecode(const uint8_t *p,size_t size,NetConfig *out) {
 if(!p || !out || size<16 || memcmp(p,"EVNET01",8))return false;
 uint32_t n=le32(p+8);if(n<422 || n>4679 || n>size-16 || Net_Crc32(p+16,n)!=le32(p+12))return false;
 uint16_t ca=(uint16_t)(p[435]|((uint16_t)p[436]<<8));
 if(ca==0 || ca>4096)return false;
 /* The optional marker enables remote control and preserves both legacy record lengths. */
 bool remote=n==422U+ca || n==583U+ca;
 if(remote && p[16U+n-1U]!=0xd3U)return false;
 if(remote)n--;
 if(n!=421U+ca && n!=582U+ca)return false;
 memset(out,0,sizeof *out);out->remote_enabled=remote;memcpy(out->host,p+16,128);memcpy(out->user,p+144,128);
 memcpy(out->password,p+272,128);memcpy(out->device,p+400,33);
 out->port=(uint16_t)(p[433]|((uint16_t)p[434]<<8));out->ca_length=ca;memcpy(out->ca,p+437,ca);
 if(!safe_at(out->host,sizeof out->host)||!safe_at(out->user,sizeof out->user)||!safe_at(out->password,sizeof out->password)||
    !memchr(out->device,0,sizeof out->device)||!identifier(out->device,32)||out->port!=8883 ||
    strncmp(out->ca,"-----BEGIN CERTIFICATE-----",27)) {memset(out,0,sizeof *out);return false;}
 if(n==582U+ca) {
  memcpy(out->legacy_topic,p+437U+ca,128);memcpy(out->room,p+565U+ca,33);
  if(!safe_at(out->legacy_topic,sizeof out->legacy_topic)||strchr(out->legacy_topic,'+')||strchr(out->legacy_topic,'#')||
     strncmp(out->legacy_topic,"classroom/",10)||!memchr(out->room,0,33)||!identifier(out->room,32)) {memset(out,0,sizeof *out);return false;}
 }
 return true;
}
static const char *boolean(bool v){return v?"true":"false";}
int Net_IPv4Profile(char *out,size_t size,const char *line) {
 const char *prefix="+QICSGP:";if(!line||!out||strncmp(line,prefix,strlen(prefix)))return -1;
 const char *p=line+strlen(prefix);while(*p==' ')p++;
 if(*p<'1'||*p>'3'||p[1]!=',')return -1;p++;const char *tail=p;
 for(unsigned i=0;i<3;i++) {
  if(*p++!=','||*p++!='"')return -1;
  size_t n=0;while(*p && *p!='"'){if((unsigned char)*p<32||(unsigned char)*p>126||*p=='\\'||++n>100)return -1;p++;}
  if(*p++!='"')return -1;
 }
 if(*p++!=','||*p<'0'||*p>'3'||p[1]!=0)return -1;
 int n=snprintf(out,size,"AT+QICSGP=1,1%s",tail);return n<0||(size_t)n>=size ? -1:n;
}
bool Net_ClockUTC(const char *line,uint32_t *utc) {
 if(!line||!utc)return false;const char *p=strchr(line,'"');if(!p)return false;p++;
 int y,m,d,h,mi,s,tz,used=0;char sign;
 if(sscanf(p,"%d/%d/%d,%d:%d:%d%c%d%n",&y,&m,&d,&h,&mi,&s,&sign,&tz,&used)!=8)return false;
 if(y<100)y+=2000;
 if(y<2025||y>2099||m<1||m>12||d<1||h<0||h>23||mi<0||mi>59||s<0||s>59||tz<0||tz>56||(sign!='+'&&sign!='-'))return false;
 if(p[used]==','){if(p[used+1]!='0'||p[used+2]!='"')return false;}
 else if(p[used]!='"')return false;
 static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 bool leap=(y%4==0&&(y%100!=0||y%400==0));
 if((unsigned)d>days[m-1]+((m==2&&leap)?1U:0U))return false;
 uint32_t total=0;for(int year=1970;year<y;year++)total+=(year%4==0&&(year%100!=0||year%400==0))?366U:365U;
 for(int month=1;month<m;month++)total+=days[month-1]+((month==2&&leap)?1U:0U);
 int64_t epoch=(int64_t)(total+(uint32_t)d-1U)*86400+h*3600+mi*60+s;
 epoch+=(sign=='-'?1:-1)*tz*900;if(epoch<0||epoch>UINT32_MAX)return false;
 *utc=(uint32_t)epoch;return true;
}
static void decimal(char *out,size_t size,int32_t value) {
 uint32_t mag=(uint32_t)(value<0?-(int64_t)value:value);
 (void)snprintf(out,size,"%s%lu.%02lu",value<0?"-":"",(unsigned long)(mag/100U),(unsigned long)(mag%100U));
}
int Net_Telemetry(char *out,size_t size,const char *device,const char *boot,const NetSample *s) {
 if(!out||!s||!identifier(device,32)||!identifier(boot,32)||s->count>20||s->occupancy>2||s->fan_level>CONTROL_FAN_MAX||
    (s->local_off && (s->remote || s->fan_level || s->led)) ||
    (s->remote ? s->hold_remaining_s==0 || s->hold_remaining_s>300 : s->hold_remaining_s!=0))return -1;
 bool vv=s->vision_valid && s->vision_age<5000U;
 bool tv=s->temp_valid && s->temp_age<10000U && s->temp_x100>=-4000 && s->temp_x100<=8500 && s->rh_x100>=0 && s->rh_x100<=10000;
 char count[12]="null",temp[24]="null",rh[24]="null",va[24]="null",ta[24]="null",ts[24]="null";
 if(vv)(void)snprintf(count,sizeof count,"%u",s->count);
 if(vv||s->have_vision)(void)snprintf(va,sizeof va,"%lu",(unsigned long)s->vision_age);
 if(tv||s->have_temp)(void)snprintf(ta,sizeof ta,"%lu",(unsigned long)s->temp_age);
 if(tv){decimal(temp,sizeof temp,s->temp_x100);decimal(rh,sizeof rh,s->rh_x100);}
 if(s->time_valid)(void)snprintf(ts,sizeof ts,"%lu",(unsigned long)s->utc);
 static const char *occupancy[]={"UNKNOWN","OCCUPIED","VACANT"};
 char last[67]="null";
 if(s->last_command[0]) {
  /* The controller saves only validated command ids after driver confirmation. */
  (void)snprintf(last,sizeof last,"\"%s\"",s->last_command);
 }
 int n=snprintf(out,size,"{\"v\":\"1.0.0\",\"device_id\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,\"uptime_ms\":%lu,\"ts\":%s,\"time_valid\":%s,\"control_epoch\":%lu,\"count\":%s,\"vision_valid\":%s,\"vision_age_ms\":%s,\"temp_c\":%s,\"temp_valid\":%s,\"temp_age_ms\":%s,\"humidity_pct\":%s,\"humidity_valid\":%s,\"occupancy\":\"%s\",\"mode\":\"%s\",\"fan_out\":%u,\"fan_level\":%u,\"led_out\":%u,\"hold_remaining_s\":%u,\"net_state\":\"ONLINE\",\"last_cmd_id\":%s,\"faults\":[%s%s%s]}",
 device,boot,(unsigned long)s->seq,(unsigned long)s->uptime,ts,boolean(s->time_valid),(unsigned long)s->control_epoch,count,boolean(vv),va,temp,boolean(tv),ta,rh,boolean(tv),occupancy[s->occupancy],s->local_off?"LOCAL_OFF":s->remote?"REMOTE":"AUTO",s->fan_level?1U:0U,(unsigned)s->fan_level,s->led?1U:0U,
 (unsigned)s->hold_remaining_s,last,
 vv?"":"\"VISION_FAULT\"",(!vv&&!tv)?",":"",tv?"":"\"SENSOR_FAULT\"");
 return n<0 || (size_t)n>=size || n>1500 ? -1:n;
}
int Net_LegacyTelemetry(char *out,size_t size,const char *device,const char *room,const NetSample *s) {
 if(!out||!s||!identifier(device,32)||!identifier(room,32)||!s->time_valid||s->utc<946684800U||!s->temp_valid||s->temp_age>=10000U||!s->light_valid||s->temp_x100<-4000||s->temp_x100>8500||s->rh_x100<0||s->rh_x100>10000)return -1;
 uint32_t days=s->utc/86400U,seconds=s->utc%86400U;unsigned year=1970,month=1;
 for(;;){unsigned count=(year%4==0&&(year%100!=0||year%400==0))?366U:365U;if(days<count)break;days-=count;year++;}
 static const unsigned months[]={31,28,31,30,31,30,31,31,30,31,30,31};
 for(;;){unsigned count=months[month-1]+((month==2&&year%4==0)?1U:0U);if(days<count)break;days-=count;month++;}
 char count[12]="null",temp[24],rh[24];decimal(temp,sizeof temp,s->temp_x100);decimal(rh,sizeof rh,s->rh_x100);
 if(s->vision_valid && s->vision_age<5000U && s->count<=20)(void)snprintf(count,sizeof count,"%u",s->count);
 int n=snprintf(out,size,"{\"schema_version\":1,\"type\":\"telemetry\",\"source\":\"stm32_c_hal_ec200u\",\"simulated\":false,\"device_id\":\"%s\",\"room_id\":\"%s\",\"seq\":%lu,\"captured_at\":\"%04u-%02u-%02uT%02lu:%02lu:%02luZ\",\"people_count\":%s,\"temperature_c\":%s,\"humidity_pct\":%s,\"light_lux\":null,\"light_raw_u16\":%u}",device,room,(unsigned long)(s->utc-946684800U),year,month,(unsigned)(days+1U),(unsigned long)(seconds/3600U),(unsigned long)(seconds%3600U/60U),(unsigned long)(seconds%60U),count,temp,rh,s->light_raw16);
 return n<0||(size_t)n>=size ? -1:n;
}
