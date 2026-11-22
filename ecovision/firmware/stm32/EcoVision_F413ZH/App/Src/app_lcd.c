/* UniKnect ST7735 shield: SPI1 PA5 SCK/PA7 MOSI, PF12 DC, PD14 CS.
 * Reset is wired to board NRST; use software reset, never drive NRST.
 */
#include "main.h"
#include "app_lcd.h"
#include "lcd_view.h"
#include "app_net.h"
static SPI_HandleTypeDef spi;
static uint8_t framebuffer[LCD_VIEW_BYTES];
static bool ready;
static bool transfer(const uint8_t *bytes,uint16_t size) {
    return !size || HAL_SPI_Transmit(&spi,(uint8_t *)bytes,size,150U)==HAL_OK;
}
static bool command(uint8_t cmd,const uint8_t *data,uint16_t count) {
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF,GPIO_PIN_12,GPIO_PIN_RESET);
    bool ok=transfer(&cmd,1);
    HAL_GPIO_WritePin(GPIOF,GPIO_PIN_12,GPIO_PIN_SET);
    if(ok) ok=transfer(data,count);
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_SET);
    return ok;
}
bool AppLcd_Init(void) {
    ready=false;
    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOD_CLK_ENABLE();__HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOF,GPIO_PIN_12,GPIO_PIN_RESET);
    GPIO_InitTypeDef pin={0};
    pin.Mode=GPIO_MODE_OUTPUT_PP;pin.Speed=GPIO_SPEED_FREQ_VERY_HIGH;pin.Pull=GPIO_NOPULL;
    pin.Pin=GPIO_PIN_14;HAL_GPIO_Init(GPIOD,&pin);
    pin.Pin=GPIO_PIN_12;HAL_GPIO_Init(GPIOF,&pin);
    pin.Mode=GPIO_MODE_AF_PP;pin.Alternate=GPIO_AF5_SPI1;pin.Pin=GPIO_PIN_5|GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA,&pin);
    spi.Instance=SPI1;spi.Init.Mode=SPI_MODE_MASTER;spi.Init.Direction=SPI_DIRECTION_1LINE;
    spi.Init.DataSize=SPI_DATASIZE_8BIT;spi.Init.CLKPolarity=SPI_POLARITY_LOW;
    spi.Init.CLKPhase=SPI_PHASE_1EDGE;spi.Init.NSS=SPI_NSS_SOFT;
    spi.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_16;spi.Init.FirstBit=SPI_FIRSTBIT_MSB;
    spi.Init.TIMode=SPI_TIMODE_DISABLE;spi.Init.CRCCalculation=SPI_CRCCALCULATION_DISABLE;
    spi.Init.CRCPolynomial=7;
    if(HAL_SPI_Init(&spi)!=HAL_OK) return false;
    if(!command(0x01,0,0)) return false;
    HAL_Delay(150);
    if(!command(0x11,0,0)) return false;
    HAL_Delay(120);
    static const uint8_t init[][18]={
        {0xb1,3,0x01,0x2c,0x2d},{0xb2,3,0x01,0x2c,0x2d},{0xb3,6,0x01,0x2c,0x2d,0x01,0x2c,0x2d},
        {0xb4,1,0x07},{0xc0,3,0xa2,0x02,0x84},{0xc1,1,0xc5},{0xc2,2,0x0a,0x00},
        {0xc3,2,0x8a,0x2a},{0xc4,2,0x8a,0xee},{0xc5,1,0x0e},
        {0x3a,1,0x05},{0x36,1,0xa8},
        {0xe0,16,0x02,0x1c,0x07,0x12,0x37,0x32,0x29,0x2d,0x29,0x25,0x2b,0x39,0x00,0x01,0x03,0x10},
        {0xe1,16,0x03,0x1d,0x07,0x06,0x2e,0x2c,0x29,0x2d,0x2e,0x2e,0x37,0x3f,0x00,0x00,0x02,0x10}
    };
    for(unsigned i=0;i<sizeof init/sizeof init[0];i++) if(!command(init[i][0],init[i]+2,init[i][1])) return false;
    if(!command(0x20,0,0) || !command(0x13,0,0) || !command(0x29,0,0)) return false;
    HAL_Delay(20);ready=true;
    return AppLcd_Show(false,false,0,0);
}
bool AppLcd_Show(bool linked,bool valid,uint8_t count,uint32_t seq) {
    LcdView_SetNetworkState(AppNet_State());
    EnvironmentSnapshot env;
    AppEnvironment_GetSnapshot(&env);
    if(!ready || !LcdView_RenderDashboard(framebuffer,sizeof framebuffer,linked,valid,count,seq,&env)) return false;
    const uint8_t cols[]={0,0,0,159},rows[]={0,0,0,127};
    if(!command(0x2a,cols,4) || !command(0x2b,rows,4) || !command(0x2c,0,0)) return false;
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF,GPIO_PIN_12,GPIO_PIN_SET);
    bool ok=transfer(framebuffer,(uint16_t)sizeof framebuffer);
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_SET);
    return ok;
}
