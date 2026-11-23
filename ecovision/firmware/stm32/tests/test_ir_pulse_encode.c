#include "ir_pulse_codec.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    IrPulseFrame f={0};uint8_t cmd=0,param=0;
    for(uint8_t t=16;t<=30;t++) {
        assert(IrPulse_Encode24(1,t,&f)&&IrPulse_ValidRaw(&f));
        assert(f.len==51&&f.us[0]==9000&&f.us[1]==4500&&f.us[50]==560);
        assert(IrPulse_Decode24(&f,&cmd,&param)&&cmd==1&&param==t);
    }
    for(uint8_t c=2;c<=8;c++) {
        const uint8_t max=c==3?4U:c==4?3U:0U;
        for(uint8_t p=(uint8_t)(max?1U:0U);p<=max;p++) {
            assert(IrPulse_Encode24(c,p,&f));
            assert(IrPulse_Decode24(&f,&cmd,&param)&&cmd==c&&param==p);
        }
    }
    assert(!IrPulse_Encode24(1,15,&f)&&!IrPulse_Encode24(1,31,&f));
    assert(!IrPulse_Encode24(3,0,&f)&&!IrPulse_Encode24(3,5,&f));
    assert(!IrPulse_Encode24(4,4,&f)&&!IrPulse_Encode24(2,1,&f));
    assert(!IrPulse_Encode24(9,0,&f)&&!IrPulse_Encode24(1,26,NULL));
    f.len=599;for(unsigned i=0;i<f.len;i++)f.us[i]=100;
    assert(IrPulse_ValidRaw(&f));
    f.us[4]=0;assert(!IrPulse_ValidRaw(&f));
    f.us[4]=15001;assert(!IrPulse_ValidRaw(&f));
    for(unsigned i=0;i<f.len;i++)f.us[i]=15000;
    assert(!IrPulse_ValidRaw(&f)); /* Bounded total transmit time. */
    f.len=600;assert(!IrPulse_ValidRaw(&f));
    f.len=601;assert(!IrPulse_ValidRaw(&f));
    assert(!IrPulse_ValidRaw(NULL));
    puts("PASS: demo commands and parameter bounds, roundtrip, mark-first raw frames, capacity and TX duration limit");
    return 0;
}
