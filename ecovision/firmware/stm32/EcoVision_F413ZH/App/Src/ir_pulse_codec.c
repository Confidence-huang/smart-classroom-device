/* EcoVision F413 port of wzy IR package v0.3's 24-bit demo protocol.
 * This is NOT a vendor air-conditioner protocol. MSB first, XOR 0x5A. */
#include "ir_pulse_codec.h"
#include <stddef.h>
static bool parameter_valid(uint8_t cmd,uint8_t param) {
    if(cmd==1U)return param>=16U&&param<=30U;
    if(cmd==3U)return param>=1U&&param<=4U;
    if(cmd==4U)return param>=1U&&param<=3U;
    return (cmd==2U||(cmd>=5U&&cmd<=8U))&&param==0U;
}
static bool between(uint16_t v,uint16_t lo,uint16_t hi){return v>=lo&&v<=hi;}
bool IrPulse_Decode24(const IrPulseFrame *f,uint8_t *cmd,uint8_t *param) {
    if(!f||f->len!=IR_PULSE_DEMO_LENGTH||!between(f->us[0],8000,10000)||
       !between(f->us[1],4000,5000)||!between(f->us[50],400,800))return false;
    uint8_t data[3]={0};
    for(unsigned i=0;i<24;i++) {
        if(!between(f->us[2+2*i],400,800))return false;
        uint16_t space=f->us[3+2*i];
        if(!between(space,400,800)&&!between(space,1400,1900))return false;
        data[i/8]=(uint8_t)((data[i/8]<<1)|(space>=1400U?1U:0U));
    }
    if((uint8_t)(data[0]^data[1]^0x5aU)!=data[2]||!parameter_valid(data[0],data[1]))return false;
    if(cmd) { *cmd=data[0]; }
    if(param) { *param=data[1]; }
    return true;
}
bool IrPulse_Encode24(uint8_t cmd,uint8_t param,IrPulseFrame *f) {
    if(!f||!parameter_valid(cmd,param))return false;
    const uint8_t data[3]={cmd,param,(uint8_t)(cmd^param^0x5aU)};
    f->us[0]=9000;f->us[1]=4500;
    for(unsigned i=0;i<24;i++) {
        f->us[2+2*i]=560;
        f->us[3+2*i]=(data[i/8]&(0x80U>>(i%8)))?1690U:560U;
    }
    f->us[50]=560;f->len=IR_PULSE_DEMO_LENGTH;return true;
}
bool IrPulse_ValidRaw(const IrPulseFrame *f) {
    if(!f||f->len<3U||f->len>IR_PULSE_CAPACITY||(f->len&1U)==0U)return false;
    uint32_t total=0;
    for(unsigned i=0;i<f->len;i++) {
        if(f->us[i]<100U||f->us[i]>15000U)return false;
        total+=f->us[i];if(total>1500000U)return false;
    }
    return true;
}
