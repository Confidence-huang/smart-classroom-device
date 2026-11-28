/**
 * @file    app_aht20.c
 * @brief   AHT20 驱动实现（I2C1：PB8=SCL / PB9=SDA，100 kHz）
 *
 * 协议（AHT20 数据手册）：
 *   1) 上电后等待 ≥ 40 ms；
 *   2) 读 1 字节状态：bit3 = 已校准(CAL)，bit7 = 忙；
 *      若未校准，发 0xBE 0x08 0x00 初始化/校准，等 10 ms；
 *   3) 触发测量：发 0xAC 0x33 0x00，等 ≥ 75 ms（这里给 80 ms）；
 *   4) 读 7 字节：[0]=状态 [1..3]=湿度 [3..5]=温度 [6]=CRC8。
 *
 * 换算（20 bit 原始值）：
 *   湿度[%RH] = raw_h / 2^20 × 100
 *   温度[℃] = raw_t / 2^20 × 200 - 50
 * 这里全部用整数运算，避免 newlib-nano/Keil 的浮点格式化问题：
 *   humi_x100 = (uint64)raw_h * 10000 >> 20
 *   temp_x100 = (int64)raw_t * 20000 >> 20 - 5000
 */
#include "main.h"
#include "app_aht20.h"
#include "environment_codec.h"
#include "cmsis_os.h"

extern I2C_HandleTypeDef hi2c1;

static void aht20_delay_ms(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning)
    {
        (void)osDelay(ms);
    }
    else
    {
        HAL_Delay(ms);
    }
}

#define AHT20_I2C_ADDR       (0x38U << 1)   /* HAL 使用 8bit 地址（左移一位） */
#define AHT20_TIMEOUT_MS     100U
#define AHT20_CMD_INIT       0xBEU
#define AHT20_CMD_TRIGGER    0xACU
#define AHT20_STATUS_CAL     0x08U          /* bit3(CAL) | bit4，正常应为 0x18 */

static int AHT20_ReadStatus(uint8_t *status)
{
    return (HAL_I2C_Master_Receive(&hi2c1, AHT20_I2C_ADDR, status, 1U,
                                   AHT20_TIMEOUT_MS) == HAL_OK) ? AHT20_OK : AHT20_ERR_NACK;
}

int AHT20_Init(void)
{
    uint8_t status = 0U;
    int ret;

    /* 1) 上电稳定：手册要求 ≥40 ms，这里给 50 ms */
    aht20_delay_ms(50U);

    /* 2) 读状态，确认总线通、传感器在校准态 */
    ret = AHT20_ReadStatus(&status);
    if (ret != AHT20_OK)
    {
        return ret;    /* 没接传感器时这里返回 -1，属于“正常异常”，上层打印后继续跑 */
    }

    if ((status & AHT20_STATUS_CAL) != AHT20_STATUS_CAL)
    {
        uint8_t cmd[3] = { AHT20_CMD_INIT, 0x08U, 0x00U };

        if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_I2C_ADDR, cmd, 3U,
                                    AHT20_TIMEOUT_MS) != HAL_OK)
        {
            return AHT20_ERR_CAL;
        }
        aht20_delay_ms(20U);

        ret = AHT20_ReadStatus(&status);
        if (ret != AHT20_OK)
        {
            return ret;
        }
        if ((status & AHT20_STATUS_CAL) != AHT20_STATUS_CAL)
        {
            return AHT20_ERR_CAL;
        }
    }

    return AHT20_OK;
}

int AHT20_Read(AHT20_SampleTypeDef *out)
{
    uint8_t cmd[3] = { AHT20_CMD_TRIGGER, 0x33U, 0x00U };
    uint8_t data[7] = { 0U };

    if (out == NULL)
    {
        return AHT20_ERR_NACK;
    }

    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_I2C_ADDR, cmd, 3U,
                                AHT20_TIMEOUT_MS) != HAL_OK)
    {
        return AHT20_ERR_NACK;
    }

    /* 单次测量耗时约 75 ms（手册典型 80 ms），使用 osDelay 释放 CPU */
    aht20_delay_ms(80U);

    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_I2C_ADDR, data, 7U,
                               AHT20_TIMEOUT_MS) != HAL_OK)
    {
        return AHT20_ERR_NACK;
    }

    return Environment_DecodeAht20(data, sizeof data, out);
}
