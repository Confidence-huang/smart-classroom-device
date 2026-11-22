/**
 * @file    app_mq135.c
 * @brief   MQ-135 采集与换算（ADC1_IN3 / PA3）
 *
 * 换算链条（全部整数运算）：
 *   1) 16 次平均 -> adc_raw
 *   2) 分压后引脚电压 v_pin_mv = adc_raw × VREF / 4095
 *   3) 传感器 AO 电压（分压前） v_ao_mv = v_pin_mv × (R1+R2) / R2
 *   4) 传感器内阻 Rs = RL × (VC - Vao) / Vao     （模块 AO = VC × RL/(RL+Rs)）
 *   5) Rs/R0 比值 = 相对污染程度（R0 = 洁净空气下的 Rs）
 *
 * 注意：RL、VC 与实际模块可能不同；本文件假设 RL=10k、VC=5.000V，
 *       换模块/换供电时要同步修改 app_mq135.h 里的宏。
 */
#include "main.h"
#include "app_mq135.h"

extern ADC_HandleTypeDef hadc1;

static uint32_t s_r0_ohm = MQ135_RL_OHM;   /* 默认 10k，可用 MQ135_CalibrateR0 覆盖 */

void MQ135_Init(void)
{
    s_r0_ohm = MQ135_RL_OHM;
}

static uint16_t MQ135_ReadAdcAverage(void)
{
    uint32_t sum = 0U;
    uint16_t valid = 0U;

    for (uint16_t i = 0U; i < MQ135_OVERSAMPLE; i++)
    {
        if (HAL_ADC_Start(&hadc1) != HAL_OK)
        {
            continue;
        }
        if (HAL_ADC_PollForConversion(&hadc1, 5U) == HAL_OK)
        {
            sum += HAL_ADC_GetValue(&hadc1);
            valid++;
        }
        (void)HAL_ADC_Stop(&hadc1);
    }

    if (valid == 0U)
    {
        return 0U;
    }
    return (uint16_t)(sum / valid);
}

void MQ135_Read(MQ135_SampleTypeDef *out)
{
    uint32_t raw;
    uint32_t v_pin_mv;
    uint32_t v_ao_mv;

    if (out == NULL)
    {
        return;
    }

    raw = MQ135_ReadAdcAverage();

    /* 2) 引脚电压（分压后） */
    v_pin_mv = ((uint32_t)raw * MQ135_VREF_MV) / MQ135_ADC_MAX;

    /* 3) 乘回分压比 (R1+R2)/R2 */
    v_ao_mv = (v_pin_mv * (MQ135_R_TOP_OHM + MQ135_R_BOTTOM_OHM)) / MQ135_R_BOTTOM_OHM;

    out->adc_raw  = raw;
    out->v_pin_mv = v_pin_mv;
    out->v_ao_mv  = v_ao_mv;
    out->r0_ohm   = s_r0_ohm;

    /* 4) Rs = RL × (VC - Vao) / Vao ；Vao 接近 0 时 Rs 会极大，直接按上限处理 */
    if (v_ao_mv == 0U)
    {
        out->rs_ohm = 0xFFFFFFFFU;
        out->rs_r0_x100 = 0x7FFFFFFF;
        return;
    }
    if (v_ao_mv >= MQ135_VC_MV)
    {
        out->rs_ohm = 0U;
        out->rs_r0_x100 = 0;
        return;
    }
    out->rs_ohm = ((uint32_t)MQ135_RL_OHM * (MQ135_VC_MV - v_ao_mv)) / v_ao_mv;

    /* 5) 比值 ×100 */
    out->rs_r0_x100 = (int32_t)((out->rs_ohm * 100U) / s_r0_ohm);
}

uint32_t MQ135_CalibrateR0(void)
{
    MQ135_SampleTypeDef s;

    MQ135_Read(&s);
    if ((s.rs_ohm == 0U) || (s.rs_ohm == 0xFFFFFFFFU))
    {
        return 0U;
    }
    s_r0_ohm = s.rs_ohm;
    return s_r0_ohm;
}

uint32_t MQ135_GetR0(void)
{
    return s_r0_ohm;
}

void MQ135_SetR0(uint32_t r0_ohm)
{
    if (r0_ohm != 0U)
    {
        s_r0_ohm = r0_ohm;
    }
}

MQ135_LevelTypeDef MQ135_LevelFromRatio(int32_t rs_r0_x100)
{
    /* 经验阈值（仅演示分级）：Rs/R0 越高空气越洁净 */
    if (rs_r0_x100 >= 150)
    {
        return MQ135_LEVEL_GOOD;
    }
    if (rs_r0_x100 >= 100)
    {
        return MQ135_LEVEL_MODERATE;
    }
    if (rs_r0_x100 >= 70)
    {
        return MQ135_LEVEL_POOR;
    }
    return MQ135_LEVEL_BAD;
}
