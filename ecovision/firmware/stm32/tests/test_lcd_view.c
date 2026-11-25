#include "lcd_view.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static uint8_t guarded[LCD_VIEW_BYTES+2], previous[LCD_VIEW_BYTES];
int main(void) {
    EnvironmentSnapshot env={0};
    memset(guarded,0xA5,sizeof guarded);
    assert(!LcdView_Render(guarded+1,LCD_VIEW_BYTES-1,true,true,12,3));
    assert(guarded[1]==0xA5);
    assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,true,true,0,3));
    memcpy(previous,guarded+1,LCD_VIEW_BYTES);
    assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,true,true,12,3));
    assert(memcmp(previous,guarded+1,LCD_VIEW_BYTES)!=0);
    memcpy(previous,guarded+1,LCD_VIEW_BYTES);
    assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,true,false,0,3));
    assert(memcmp(previous,guarded+1,LCD_VIEW_BYTES)!=0);
    memcpy(previous,guarded+1,LCD_VIEW_BYTES);
    assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,true,true,255,3));
    assert(memcmp(previous,guarded+1,LCD_VIEW_BYTES)==0);
    assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,false,false,0,3));
    assert(memcmp(previous,guarded+1,LCD_VIEW_BYTES)!=0);
    for(uint8_t n=0;n<=20;n++) assert(LcdView_Render(guarded+1,LCD_VIEW_BYTES,true,true,n,0xffffffffU));
    assert(guarded[0]==0xA5 && guarded[LCD_VIEW_BYTES+1]==0xA5);
    env.th_valid=true;env.light_valid=true;env.temp_x100=2510;env.humi_x100=5030;env.light_raw16=23029;
    assert(LcdView_RenderDashboard(guarded+1,LCD_VIEW_BYTES,true,true,12,583,&env));
    /* CI writes to its temporary directory, while the desktop script keeps its preview path. */
    const char *preview_path=getenv("ECOVISION_LCD_PREVIEW");
    FILE *preview=fopen(preview_path ? preview_path : "firmware/stm32/environment-evidence/dashboard-preview.ppm","wb");
    assert(preview!=NULL);
    (void)fprintf(preview,"P6\n160 128\n255\n");
    for(size_t at=1;at<LCD_VIEW_BYTES+1;at+=2) {
        uint16_t color=(uint16_t)(((uint16_t)guarded[at]<<8)|guarded[at+1]);
        uint8_t rgb[3]={(uint8_t)(((color>>11)&31U)*255U/31U),(uint8_t)(((color>>5)&63U)*255U/63U),(uint8_t)((color&31U)*255U/31U)};
        (void)fwrite(rgb,1,3,preview);
    }
    (void)fclose(preview);
    memset(&env,0,sizeof env);
    assert(LcdView_RenderDashboard(guarded+1,LCD_VIEW_BYTES,true,true,12,3,&env));
    memcpy(previous,guarded+1,LCD_VIEW_BYTES);
    env.th_valid=true;env.light_valid=true;env.temp_x100=0;env.humi_x100=0;env.light_raw16=0;
    assert(LcdView_RenderDashboard(guarded+1,LCD_VIEW_BYTES,true,true,12,3,&env));
    assert(memcmp(previous,guarded+1,LCD_VIEW_BYTES)!=0);
    env.temp_x100=-40;env.humi_x100=10000;env.light_raw16=65535;
    assert(LcdView_RenderDashboard(guarded+1,LCD_VIEW_BYTES,true,true,20,0xffffffffU,&env));
    assert(guarded[0]==0xA5 && guarded[LCD_VIEW_BYTES+1]==0xA5);
    puts("PASS: LCD render bounds, valid zero, count 12, invalid placeholder, wait state, out-of-range suppression, 0..20 and uint32 seq");
    return 0;
}
