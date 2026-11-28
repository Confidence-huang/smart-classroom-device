#include "main.h"
#include "app_environment.h"
#ifdef ECOVISION_DAYLIGHT_SHADOW
#include "app_control.h"
#endif
#include "app_aht20.h"
#include "environment_codec.h"
#include "app_log.h"
#include "app_light.h"
#include "app_bh1750.h"
#include "app_sgp30.h"
#include "app_alert.h"
#include "cmsis_os.h"
#include <string.h>

typedef struct {
    int32_t buf[3];
    uint8_t count, idx;
} Median3;

static void median3_reset(Median3 *m) {
    m->count = 0U;
    m->idx = 0U;
}

static int32_t median3_push(Median3 *m, int32_t v) {
    m->buf[m->idx] = v;
    m->idx = (uint8_t)((m->idx + 1U) % 3U);
    if (m->count < 3U) {
        ++m->count;
        return v;
    }
    int32_t a = m->buf[0], b = m->buf[1], c = m->buf[2], t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}
static EnvironmentSnapshot latest;
extern I2C_HandleTypeDef hi2c1;
static void i2c1_delay_us(void) {
    for(volatile unsigned i=0;i<200U;++i) {}
}
static void i2c1_recover_bus(void) {
    (void)HAL_I2C_DeInit(&hi2c1);
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef pin={0};
    pin.Pin=GPIO_PIN_8|GPIO_PIN_9;
    pin.Mode=GPIO_MODE_OUTPUT_OD;
    pin.Pull=GPIO_NOPULL;
    pin.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8|GPIO_PIN_9,GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB,&pin);
    i2c1_delay_us();
    unsigned scl_before=(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8)==GPIO_PIN_SET)?1U:0U;
    unsigned sda_before=(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)==GPIO_PIN_SET)?1U:0U;
    for(unsigned i=0;i<9U;++i) {
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_RESET);
        i2c1_delay_us();
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);
        i2c1_delay_us();
    }
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);
    i2c1_delay_us();
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);
    i2c1_delay_us();
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_SET);
    i2c1_delay_us();
    unsigned scl_after=(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8)==GPIO_PIN_SET)?1U:0U;
    unsigned sda_after=(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)==GPIO_PIN_SET)?1U:0U;
    __HAL_RCC_I2C1_FORCE_RESET();
    i2c1_delay_us();
    __HAL_RCC_I2C1_RELEASE_RESET();
    (void)HAL_I2C_Init(&hi2c1);
    unsigned busy=(__HAL_I2C_GET_FLAG(&hi2c1,I2C_FLAG_BUSY)!=RESET)?1U:0U;
    AppLog_Printf("[I2C1] recover before=scl:%u,sda:%u after=scl:%u,sda:%u busy=%u\r\n",
        scl_before,sda_before,scl_after,sda_after,busy);
}
void AppEnvironment_GetSnapshot(EnvironmentSnapshot *out) {
    taskENTER_CRITICAL();*out=latest;taskEXIT_CRITICAL();
    if((uint32_t)(HAL_GetTick()-out->sampled_at)>=10000U) {
        out->th_valid=false;out->light_valid=false;out->lux_valid=false;
    }
    if ((uint32_t)(HAL_GetTick()-out->air_sampled_at)>=10000U) out->air_valid=false;
}
void AppEnvironment_Run(void *argument) {
    (void)argument;osDelay(700U);
    AppLight_Init();
    bool initialized=false;
    Bh1750_Context bh1750={0};
    Sgp30_Context sgp30={0};
    Median3 temp_flt={0}, humi_flt={0}, eco2_flt={0}, tvoc_flt={0};
    uint32_t cycle_at=osKernelGetTickCount();
    uint32_t previous_log=HAL_GetTick()-5000U;
    uint32_t last_recover=HAL_GetTick()-5000U;
    unsigned bus_fail_streak=0U;
    if(__HAL_I2C_GET_FLAG(&hi2c1,I2C_FLAG_BUSY)!=RESET) {
        i2c1_recover_bus();
        last_recover=HAL_GetTick();
    }
    for(;;) {
        AppHealth_Beat(HEALTH_ENVIRONMENT);
        EnvironmentSnapshot next;memset(&next,0,sizeof next);
        Sgp30_Sample air={0};
        next.air_status=Sgp30_Poll(&sgp30,&hi2c1,&air);
        if (next.air_status==SGP30_OK) {
            next.air_valid=true;
            next.air_sampled_at=HAL_GetTick();
            next.eco2_ppm=(uint16_t)median3_push(&eco2_flt,(int32_t)air.eco2_ppm);
            next.tvoc_ppb=(uint16_t)median3_push(&tvoc_flt,(int32_t)air.tvoc_ppb);
        } else if (next.air_status==SGP30_BAD_CRC && latest.air_valid &&
                   (uint32_t)(HAL_GetTick()-latest.air_sampled_at)<10000U) {
            next.air_valid=true;
            next.air_sampled_at=latest.air_sampled_at;
            next.eco2_ppm=latest.eco2_ppm;
            next.tvoc_ppb=latest.tvoc_ppb;
        } else if (next.air_status!=SGP30_BAD_CRC) {
            median3_reset(&eco2_flt);
            median3_reset(&tvoc_flt);
        }
        Bh1750_Sample light={0};
        next.light_error=Bh1750_Poll(&bh1750,&hi2c1,&light);
        next.lux_valid=next.light_error==BH1750_OK;
        next.bh1750_raw=light.raw;next.lux_x100=light.lux_x100;
        if(!initialized) initialized=AHT20_Init()==AHT20_OK;
        AHT20_SampleTypeDef sample;
        next.sensor_error=initialized?AHT20_Read(&sample):AHT20_ERR_NACK;
        if(next.sensor_error==AHT20_OK) {
            next.th_valid=true;
            next.temp_x100=median3_push(&temp_flt,sample.temp_x100);
            next.humi_x100=median3_push(&humi_flt,sample.humi_x100);
            bus_fail_streak=0U;
        } else {
            initialized=false;
            median3_reset(&temp_flt);
            median3_reset(&humi_flt);
            if(!next.lux_valid && !next.air_valid) {
                ++bus_fail_streak;
                if(bus_fail_streak>=2U && (uint32_t)(HAL_GetTick()-last_recover)>=5000U) {
                    i2c1_recover_bus();
                    last_recover=HAL_GetTick();
                    bus_fail_streak=0U;
                    memset(&bh1750,0,sizeof bh1750);
                    memset(&sgp30,0,sizeof sgp30);
                }
            } else {
                bus_fail_streak=0U;
            }
        }
        next.light_valid=AppLight_ReadRaw12(&next.light_raw12);
        if(next.light_valid) next.light_raw16=Environment_Adc12ToU16(next.light_raw12);
        next.sampled_at=HAL_GetTick();
        taskENTER_CRITICAL();latest=next;taskEXIT_CRITICAL();
#ifdef ECOVISION_DAYLIGHT_SHADOW
        AppControl_LogDaylight();
#endif
        if(next.sampled_at-previous_log>=5000U) {
            AppLog_Printf("[ENV] th_valid=%u temp_x100=%ld rh_x100=%ld adc_valid=%u adc12=%u adc16=%u err=%d\r\n",
                next.th_valid?1U:0U,(long)next.temp_x100,(long)next.humi_x100,next.light_valid?1U:0U,
                next.light_raw12,next.light_raw16,next.sensor_error);
            previous_log=next.sampled_at;
            AppLog_Printf("[LIGHT] source=BH1750 addr=0x%02X valid=%u raw=%u lux=%lu.%02lu status=%d\r\n",
                (unsigned)bh1750.address7,next.lux_valid?1U:0U,(unsigned)next.bh1750_raw,
                (unsigned long)(next.lux_x100/100U),(unsigned long)(next.lux_x100%100U),next.light_error);
            if (next.air_valid) {
                AppLog_Printf("[AIR] source=SGP30 valid=1 tvoc_ppb=%u eco2_ppm=%u\r\n",
                    (unsigned)next.tvoc_ppb,(unsigned)next.eco2_ppm);
            } else {
                AppLog_Printf("[AIR] source=SGP30 valid=0 status=%d raw=%02X%02X%02X_%02X%02X%02X\r\n",
                    next.air_status,sgp30.last_raw[0],sgp30.last_raw[1],sgp30.last_raw[2],
                    sgp30.last_raw[3],sgp30.last_raw[4],sgp30.last_raw[5]);
            }
        }
        cycle_at+=1000U;
        if (osDelayUntil(cycle_at)!=osOK) {
            osDelay(1U);
            cycle_at=osKernelGetTickCount();
        }
    }
}
