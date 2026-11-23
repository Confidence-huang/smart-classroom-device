#include "ir_pulse_codec.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    IrPulseFrame f={0}; uint8_t cmd=0,param=0;
    const uint8_t bytes[]={1U,26U,1U^26U^0x5aU};
    f.us[0]=9000;f.us[1]=4500;f.len=51;
    for(unsigned bit=0;bit<24;bit++){
        f.us[2+2*bit]=560;
        f.us[3+2*bit]=(bytes[bit/8]&(0x80U>>(bit%8)))?1690:560;
    }
    f.us[50]=560;
    assert(IrPulse_Decode24(&f,&cmd,&param) && cmd==1 && param==26);
    f.us[17]=65535; /* A malformed '1' space must not pass just because it exceeds 1000. */
    assert(!IrPulse_Decode24(&f,&cmd,&param));
    f.us[17]=1690; f.us[50]=10;
    assert(!IrPulse_Decode24(&f,&cmd,&param));
    f.us[50]=560;f.len=50;assert(!IrPulse_Decode24(&f,&cmd,&param));
    f.len=52;assert(!IrPulse_Decode24(&f,&cmd,&param));
    f.len=51;f.us[3]=1690;assert(!IrPulse_Decode24(&f,&cmd,&param));
    assert(!IrPulse_Decode24(NULL,&cmd,&param));
    puts("PASS: donor-compatible vector, bounded bit spaces, stop mark, exact length, XOR and null");
    return 0;
}
