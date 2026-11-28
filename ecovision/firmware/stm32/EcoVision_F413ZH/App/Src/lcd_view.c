#include "lcd_view.h"
#include <string.h>
#include <stdio.h>
static const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-/ .%";
static const uint8_t glyph[][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
 {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},
 {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},
 {0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},
 {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x36,0x36,0,0},{8,8,8,8,8},{0x20,0x10,8,4,2},{0,0,0,0,0},
 {0,0x60,0x60,0,0},{0x63,0x13,8,0x64,0x63}
};
static void text(uint8_t *pixels,unsigned x,unsigned y,const char *s,unsigned scale,uint16_t color) {
    for(;*s;s++,x+=6U*scale) {
        const char *found=strchr(alphabet,*s);
        if(!found) continue;
        const uint8_t *g=glyph[(size_t)(found-alphabet)];
        for(unsigned col=0;col<5;col++) for(unsigned row=0;row<7;row++) if(g[col]&(1U<<row)) {
            for(unsigned dx=0;dx<scale;dx++) for(unsigned dy=0;dy<scale;dy++) {
                unsigned px=x+col*scale+dx,py=y+row*scale+dy;
                if(px<LCD_VIEW_WIDTH && py<LCD_VIEW_HEIGHT) {
                    size_t at=((size_t)py*LCD_VIEW_WIDTH+px)*2;
                    pixels[at]=(uint8_t)(color>>8);pixels[at+1]=(uint8_t)color;
                }
            }
        }
    }
}
static char network_state[13]="NOTINIT";
void LcdView_SetNetworkState(const char *state) {
    (void)snprintf(network_state,sizeof network_state,"%.12s",state?state:"UNKNOWN");
}
bool LcdView_Render(uint8_t *pixels,size_t size,bool linked,bool valid,uint8_t count,uint32_t seq) {
    return LcdView_RenderDashboard(pixels,size,linked,valid,count,seq,0);
}
static void fixed_one(char *out,size_t size,const char *label,int32_t value,const char *unit) {
    uint32_t magnitude=(uint32_t)(value<0?-(int64_t)value:value);
    uint32_t tenths=(magnitude+5U)/10U;
    (void)snprintf(out,size,"%s%s%lu.%lu%s",label,value<0?"-":"",(unsigned long)(tenths/10U),(unsigned long)(tenths%10U),unit);
}
static const char *air_quality_label(const EnvironmentSnapshot *env,uint16_t *color) {
    if(!env || !env->air_valid) {
        *color=0xfd20;
        return (env && env->air_status==1)?"WARM":"ERR";
    }
    if(env->eco2_ppm>=2000U || env->tvoc_ppb>=660U) {
        *color=0xf800;
        return "POOR";
    }
    if(env->eco2_ppm>=1000U || env->tvoc_ppb>=220U) {
        *color=0xffe0;
        return "FAIR";
    }
    *color=0x07e0;
    return "GOOD";
}
bool LcdView_RenderDashboard(uint8_t *pixels,size_t size,bool linked,bool valid,uint8_t count,uint32_t seq,const EnvironmentSnapshot *env) {
    if(!pixels || size<LCD_VIEW_BYTES) return false;
    memset(pixels,0,LCD_VIEW_BYTES);
    bool good=linked && valid && count<=20;
    text(pixels,4,2,"ECOVISION / K230",1,0x07ff);
    char line[32]="PEOPLE: --";
    if(good) (void)snprintf(line,sizeof line,"PEOPLE: %u",(unsigned)count);
    text(pixels,4,12,line,2,good?0x07e0:0xfd20);
    bool th=env && env->th_valid && env->temp_x100>=-4000 && env->temp_x100<=8500 && env->humi_x100>=0 && env->humi_x100<=10000;
    if(th) fixed_one(line,sizeof line,"TEMP:",env->temp_x100," C");
    else (void)snprintf(line,sizeof line,"TEMP: -- C");
    text(pixels,4,30,line,2,th?0xffff:0xfd20);
    if(th) fixed_one(line,sizeof line,"RH:",env->humi_x100," %");
    else (void)snprintf(line,sizeof line,"RH: -- %%");
    text(pixels,4,48,line,2,th?0xffff:0xfd20);
    uint16_t air_color=0xfd20;
    const char *air_label=air_quality_label(env,&air_color);
    bool air=env && env->air_valid;
    if(air) (void)snprintf(line,sizeof line,"ECO2:%u",(unsigned)env->eco2_ppm);
    else (void)snprintf(line,sizeof line,"ECO2:--");
    text(pixels,4,66,line,2,air_color);
    if(air) (void)snprintf(line,sizeof line,"TVOC:%u",(unsigned)env->tvoc_ppb);
    else (void)snprintf(line,sizeof line,"TVOC:--");
    text(pixels,4,84,line,2,air_color);
    if(env && env->lux_valid) (void)snprintf(line,sizeof line,"AIR:%s LUX:%lu",air_label,(unsigned long)(env->lux_x100/100U));
    else (void)snprintf(line,sizeof line,"AIR:%s LUX:--",air_label);
    text(pixels,4,102,line,1,air_color);
    (void)snprintf(line,sizeof line,"UART:%s NET:%s",!linked?"WAIT":good?"OK":"BAD",network_state);
    text(pixels,4,112,line,1,!strcmp(network_state,"ONLINE")?0x07e0:0xfd20);
    (void)snprintf(line,sizeof line,"SEQ: %lu",(unsigned long)seq);
    text(pixels,4,121,line,1,0xffff);
    return true;
}
