#include "app_bh1750.h"
#include "lcd_view.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t tick;
static uint8_t address = 0x23U, commands[16];
static unsigned probes, writes, reads;
static bool fail_tx, fail_rx;
static uint16_t raw;
uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *b,uint16_t a,uint32_t trials,uint32_t timeout)
{ assert(b && trials==1U && timeout==10U); ++probes; return a==(uint16_t)(address<<1)?HAL_OK:HAL_ERROR; }
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *b,uint16_t a,uint8_t *p,uint16_t n,uint32_t timeout)
{ assert(b && a==(uint16_t)(address<<1) && n==1U && timeout==10U); commands[writes++]=*p; return fail_tx?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *b,uint16_t a,uint8_t *p,uint16_t n,uint32_t timeout)
{ assert(b && a==(uint16_t)(address<<1) && n==2U && timeout==10U); ++reads; if(fail_rx)return HAL_ERROR; p[0]=(uint8_t)(raw>>8);p[1]=(uint8_t)raw;return HAL_OK; }
int main(void)
{
    I2C_HandleTypeDef bus={0}; Bh1750_Context ctx={0}; Bh1750_Sample sample={0};
    const uint8_t expected[]={1U,7U,0x42U,0x65U,0x10U};
    assert(Bh1750_Poll(NULL,&bus,&sample)==BH1750_IO_ERROR);
    assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_PENDING);
    assert(probes==1U && writes==5U && memcmp(commands,expected,sizeof expected)==0);
    tick=179U;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_PENDING && reads==0U);
    tick=180U;raw=1200U;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_OK);
    assert(sample.raw==1200U && sample.lux_x100==100000U);
    tick=360U;raw=0;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_OK && sample.lux_x100==0U);
    tick=540U;raw=65535U;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_SATURATED);
    tick=720U;fail_rx=true;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_IO_ERROR && !ctx.configured);
    unsigned old_probes=probes; tick=2719U;assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_PENDING && probes==old_probes);
    ctx=(Bh1750_Context){0}; address=0x5cU;writes=0;fail_rx=false;
    assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_PENDING && ctx.address7==0x5cU);
    ctx=(Bh1750_Context){0}; address=0;old_probes=probes;
    assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_IO_ERROR && probes==old_probes+2U);
    ctx=(Bh1750_Context){0};address=0x23;writes=0;fail_tx=true;
    assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_IO_ERROR && !ctx.configured);
    fail_tx=false;ctx=(Bh1750_Context){.attempted=true,.configured=true,.address7=0x23,.last_action=UINT32_MAX-100U};
    tick=79U;raw=0x8390U;
    assert(Bh1750_Poll(&ctx,&bus,&sample)==BH1750_OK && sample.lux_x100==2806667U);
    static uint8_t pixels[LCD_VIEW_BYTES+2], previous[LCD_VIEW_BYTES];
    EnvironmentSnapshot env={0};memset(pixels,0xa5,sizeof pixels);
    assert(LcdView_RenderDashboard(pixels+1,LCD_VIEW_BYTES,true,true,12,3,&env));
    memcpy(previous,pixels+1,LCD_VIEW_BYTES);
    env.lux_valid=true;env.lux_x100=0;
    assert(LcdView_RenderDashboard(pixels+1,LCD_VIEW_BYTES,true,true,12,3,&env));
    assert(memcmp(previous,pixels+1,LCD_VIEW_BYTES)!=0);
    env.lux_x100=5461250U;
    assert(LcdView_RenderDashboard(pixels+1,LCD_VIEW_BYTES,true,true,12,3,&env));
    assert(pixels[0]==0xa5 && pixels[LCD_VIEW_BYTES+1]==0xa5);
    puts("PASS: command sequence, addresses, 180ms gate, lux conversion, valid darkness, saturation, failures, retry backoff, tick wrap, LCD invalid/zero/bounds");
    return 0;
}
