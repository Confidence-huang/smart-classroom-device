#include "vision_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void send(EvVision *v, const char *body, uint32_t now)
{
    char packet[160];
    int n = snprintf(packet, sizeof packet, "$%s*%04X\r\n", body,
                     (unsigned)EvVision_Crc((const uint8_t *)body, strlen(body)));
    for (int i = 0; i < n; ++i) EvVision_Feed(v, (uint8_t)packet[i], now);
}
int main(void)
{
    EvVision v;
    EvVision_Init(&v, "000000000000000a", 0);
    assert(EvVision_Crc((const uint8_t *)"123456789",9) == 0x29B1);
    send(&v,"EV1,000000000000000a,000000000000000b,0,0,12,1",10);
    assert(!v.active && !v.valid);
    send(&v,"EVH,000000000000000c,000000000000000b",20);
    assert(!v.active);
    send(&v,"EVH,000000000000000a,000000000000000b",30);
    assert(v.active && !v.have_seq && !v.valid);
    send(&v,"EV1,000000000000000a,000000000000000b,4294967295,1000,12,1",100);
    assert(v.valid && v.count==12 && v.accepted==1);
    send(&v,"EV1,000000000000000a,000000000000000b,0,2000,0,1",1100);
    assert(v.valid && v.count==0 && v.accepted==2);
    send(&v,"EVH,000000000000000a,000000000000000b",1200);
    send(&v,"EV1,000000000000000a,000000000000000b,0,2000,9,1",1300);
    assert(v.accepted==2 && v.count==0 && v.fresh_at==1100);
    send(&v,"EV1,000000000000000a,000000000000000b,4294967295,2000,9,1",1400);
    assert(v.accepted==2);
    send(&v,"EV1,000000000000000a,000000000000000b,1,3000,0,0",2100);
    assert(!v.valid && v.accepted==3 && v.fresh_at==1100);
    assert(v.invalid_frames==1);
    send(&v,"EV1,000000000000000a,000000000000000b,2,4000,21,1",2200);
    send(&v,"EV1,000000000000000a,000000000000000b,2,4000,1,0",2200);
    send(&v,"EV1,000000000000000a,000000000000000b,02,4000,1,1",2200);
    send(&v,"EV1,000000000000000a,000000000000000b,4294967296,4000,1,1",2200);
    assert(v.accepted==3);
    const char *bad="$EV1,000000000000000a,000000000000000b,2,4000,3,1*0000\r\n";
    for(size_t i=0;i<strlen(bad);i++) EvVision_Feed(&v,(uint8_t)bad[i],2300);
    assert(v.accepted==3);
    EvVision_Feed(&v,'$',2400); EvVision_Feed(&v,'E',2600);
    assert(v.used==0);
    EvVision_Feed(&v,'$',2700);
    for(int i=0;i<140;i++) EvVision_Feed(&v,'X',2700);
    send(&v,"EV1,000000000000000a,000000000000000b,2,5000,20,1",3100);
    assert(v.valid && v.count==20 && v.accepted==4);
    assert(v.invalid_frames==1); /* A valid frame must not hide the earlier interruption from AUTO. */
    assert(!EvVision_Expired(&v,8099) && EvVision_Expired(&v,8100));
    EvVision_Init(&v,"000000000000000d",0xfffffff0U);
    assert(v.invalid_frames==0);
    send(&v,"EVH,000000000000000d,000000000000000b",0xfffffff0U);
    assert(!EvVision_Expired(&v,0x20U));
    send(&v,"EV1,000000000000000a,000000000000000b,3,6000,12,1",0x30U);
    assert(v.accepted==0);
    puts("PASS: CRC, handshake, count 0/12/20, faults, sequence wrap, replay, range, canonical numbers, bad CRC, timeout, overflow, resync, tick wrap");
    return 0;
}
