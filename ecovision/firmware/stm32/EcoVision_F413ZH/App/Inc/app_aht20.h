/**
 * @file    app_aht20.h
 * @brief   AHT20 温湿度传感器驱动（I2C，7bit 地址 0x38）
 *
 * 接线（NUCLEO-F413ZH + ST Zio 排针，见 docs/01_硬件选型与接线.md）：
 *   AHT20 SCL -> CN7 pin2 (D15) -> PB8 = I2C1_SCL
 *   AHT20 SDA -> CN7 pin4 (D14) -> PB9 = I2C1_SDA
 *   AHT20 VCC -> CN8 pin7 (+3.3V)   AHT20 GND -> CN8 pin11 (GND)
 *
 * 量纲约定：温度 ×100（2504 = 25.04 ℃），湿度 ×100（4867 = 48.67 %RH）。
 */
#ifndef APP_AHT20_H
#define APP_AHT20_H

#include <stdint.h>

/** AHT20 一帧测量结果 */
typedef struct
{
    int32_t temp_x100;   /**< 温度 ×100 ℃，典型 -4000..8000（-40..80 ℃，传感器量程 -40..85 ℃） */
    int32_t humi_x100;   /**< 相对湿度 ×100 %RH，0..10000 */
} AHT20_SampleTypeDef;

/** 返回值 / 错误码 */
#define AHT20_OK            0
#define AHT20_ERR_NACK     -1   /**< 从机无应答：没接传感器 / 接线错 / 上拉缺失 / 地址不对 */
#define AHT20_ERR_TIMEOUT  -2   /**< 总线超时（SDA 被拉死，可参考总线恢复流程） */
#define AHT20_ERR_BUSY     -3   /**< 读到的状态为 busy（测量未完成） */
#define AHT20_ERR_CAL      -4   /**< 传感器未校准且初始化命令失败 */
#define AHT20_ERR_CRC      -5
#define AHT20_ERR_RANGE    -6

/**
 * @brief  上电初始化：等待 ≥40 ms 稳定，读状态，必要时发送 0xBE 0x08 0x00 校准
 * @return AHT20_OK 或错误码
 * @note   阻塞约 50~70 ms；建议在 main 里 mx 初始化之后调用一次
 */
int AHT20_Init(void);

/**
 * @brief  触发一次测量并读回结果（阻塞约 80 ms）
 * @param  out 结果指针（不可为 NULL）
 * @return AHT20_OK 或错误码
 */
int AHT20_Read(AHT20_SampleTypeDef *out);

#endif /* APP_AHT20_H */
