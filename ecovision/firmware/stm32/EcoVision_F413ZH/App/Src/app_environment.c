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
#include "cmsis_os.h"
#include <string.h>
static EnvironmentSnapshot latest;
extern I2C_HandleTypeDef hi2c1;
void AppEnvironment_GetSnapshot(EnvironmentSnapshot *out) {
    taskENTER_CRITICAL();*out=latest;taskEXIT_CRITICAL();
    if((uint32_t)(HAL_GetTick()-out->sampled_at)>=10000U) {
        out->th_valid=false;out->light_valid=false;out->lux_valid=false;
    }
    if ((uint32_t)(HAL_GetTick()-out->air_sampled_at)>=2500U) out->air_valid=false;
}
void AppEnvironment_Run(void *argument) {
    (void)argument;osDelay(700U);
    AppLight_Init();
    bool initialized=false;
    Bh1750_Context bh1750={0};
    Sgp30_Context sgp30={0};
    uint32_t cycle_at=osKernelGetTickCount();
    uint32_t previous_log=HAL_GetTick()-5000U;
    for(;;) {
        EnvironmentSnapshot next;memset(&next,0,sizeof next);
        Sgp30_Sample air={0};
        next.air_status=Sgp30_Poll(&sgp30,&hi2c1,&air);
        if (next.air_status==SGP30_OK) {
            next.air_valid=true;
            next.air_sampled_at=HAL_GetTick();
            next.eco2_ppm=air.eco2_ppm;
            next.tvoc_ppb=air.tvoc_ppb;
        }
        Bh1750_Sample light={0};
        next.light_error=Bh1750_Poll(&bh1750,&hi2c1,&light);
        next.lux_valid=next.light_error==BH1750_OK;
        next.bh1750_raw=light.raw;next.lux_x100=light.lux_x100;
        if(!initialized) initialized=AHT20_Init()==AHT20_OK;
        AHT20_SampleTypeDef sample;
        next.sensor_error=initialized?AHT20_Read(&sample):AHT20_ERR_NACK;
        if(next.sensor_error==AHT20_OK) {
            next.th_valid=true;next.temp_x100=sample.temp_x100;next.humi_x100=sample.humi_x100;
        } else initialized=false;
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
                AppLog_Printf("[AIR] source=SGP30 valid=0 status=%d\r\n",next.air_status);
            }
        }
        cycle_at+=1000U;
        if (osDelayUntil(cycle_at)!=osOK) {
            osDelay(1U);
            cycle_at=osKernelGetTickCount();
        }
    }
}
