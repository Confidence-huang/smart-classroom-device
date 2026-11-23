#include "vision_protocol.h"
#include <string.h>

uint16_t EvVision_Crc(const uint8_t *data, size_t size)
{
    uint16_t crc=0xffffU;
    for(size_t i=0;i<size;i++) {
        crc ^= (uint16_t)((uint16_t)data[i]<<8);
        for(unsigned b=0;b<8;b++) crc=(uint16_t)((crc&0x8000U) ? (crc<<1)^0x1021U : crc<<1);
    }
    return crc;
}
static bool identifier(const char *s)
{
    if(strlen(s)!=16U) return false;
    for(unsigned i=0;i<16;i++) if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))) return false;
    return true;
}
static bool number(const char *s,uint32_t *value)
{
    uint32_t n=0;
    if(!*s || (s[0]=='0' && s[1])) return false;
    for(;*s;s++) {
        if(*s<'0'||*s>'9') return false;
        unsigned digit=(unsigned)(*s-'0');
        if(n>(UINT32_MAX-digit)/10U) return false;
        n=n*10U+digit;
    }
    *value=n;return true;
}
void EvVision_Init(EvVision *v,const char *link,uint32_t now)
{
    memset(v,0,sizeof *v);
    memcpy(v->link,link,16);v->fresh_at=now;
}
bool EvVision_Expired(const EvVision *v,uint32_t now)
{
    return v->active && (uint32_t)(now-v->fresh_at)>=5000U;
}
static void parse(EvVision *v,uint32_t now)
{
    size_t n=v->used;
    if(n<8 || v->buffer[n-2]!='\r' || v->buffer[n-1]!='\n' || v->buffer[n-7]!='*') {v->errors++;return;}
    uint16_t supplied=0;
    for(size_t i=n-6;i<n-2;i++) {
        char c=v->buffer[i];
        if(!((c>='0'&&c<='9')||(c>='A'&&c<='F'))) {v->errors++;return;}
        supplied=(uint16_t)((supplied<<4)|(uint16_t)(c<='9'?c-'0':c-'A'+10));
    }
    if(supplied!=EvVision_Crc((const uint8_t *)v->buffer+1,n-8)) {v->errors++;return;}
    v->buffer[n-7]='\0';
    char *fields[8];unsigned count=1;fields[0]=v->buffer+1;
    for(char *p=v->buffer+1;*p;p++) if(*p==',') {
        *p='\0';if(count==8) {v->errors++;return;}fields[count++]=p+1;
    }
    if(count<3 || !identifier(fields[1]) || !identifier(fields[2])) {v->errors++;return;}
    if(strcmp(fields[1],v->link)) {v->rejected++;return;}
    if(!strcmp(fields[0],"EVH") && count==3) {
        if(!v->active) {memcpy(v->boot,fields[2],17);v->active=true;v->fresh_at=now;}
        else v->rejected++;
        return;
    }
    uint32_t seq,uptime,people,valid;
    if(count!=7 || strcmp(fields[0],"EV1") || !number(fields[3],&seq) ||
       !number(fields[4],&uptime) || !number(fields[5],&people) || !number(fields[6],&valid) ||
       people>20 || valid>1 || (!valid && people)) {v->errors++;return;}
    if(!v->active || strcmp(fields[2],v->boot) ||
       (v->have_seq && ((uint32_t)(seq-v->seq)==0 || (uint32_t)(seq-v->seq)>=0x80000000U))) {v->rejected++;return;}
    v->have_seq=true;v->seq=seq;v->uptime=uptime;v->count=(uint8_t)people;v->valid=valid!=0;v->accepted++;
    if(v->valid) v->fresh_at=now;
    else v->invalid_frames++; /* Keep the interruption visible after the next valid frame replaces this sample. */
}
void EvVision_Feed(EvVision *v,uint8_t byte,uint32_t now)
{
    if(v->used && (uint32_t)(now-v->start_at)>=200U) {v->used=0;v->errors++;}
    if(byte=='$') {v->used=1;v->buffer[0]='$';v->start_at=now;return;}
    if(!v->used) return;
    if(byte>127 || v->used>=128) {v->used=0;v->errors++;return;}
    v->buffer[v->used++]=(char)byte;
    if(byte=='\n') {v->buffer[v->used]='\0';parse(v,now);v->used=0;}
}
