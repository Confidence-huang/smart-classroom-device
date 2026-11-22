/**
 * @file    app_mq135.h
 * @brief   MQ-135 空气质量（模拟量）驱动：ADC1_IN3 / PA3，5V 模块 + 10k/15k 分压
 *
 * 接线（见 docs/01_硬件选型与接线.md）：
 *   MQ-135 AO --[10k]--+-- PA3 (CN9 pin1 / A0，ADC1_IN3)
 *                      |
 *                    [15k]
 *                      |
 *                     GND
 *   MQ-135 VCC -> CN8 pin9 (+5V)   MQ-135 GND -> CN8 pin13 (GND)
 *   分压原因：模块 5V 供电时 AO 可接近 5V，而 PA3 是模拟输入，模拟态不耐 5V，
 *             且 ADC 量程上限 = VREF+ = 3.3V。10k/15k 使 5V -> 3.0V。
 *
 * 重要：MQ-135 读数是“相对趋势”，不是 ppm 绝对值。
 *   需要 burn-in（首次 24~48 h）与每次上电预热（≥3 分钟），再用洁净空气标定 R0。
 */
#ifndef APP_MQ135_H
#define APP_MQ135_H

#include <stdint.h>

/* ---------- 硬件参数（按实际分压电阻修改） ---------- */
#define MQ135_R_TOP_OHM        10000U   /**< 分压上臂 R1 */
#define MQ135_R_BOTTOM_OHM     15000U   /**< 分压下臂 R2 */
#define MQ135_RL_OHM           10000U   /**< 模块板载负载电阻 RL（常见 10k，看模块丝印） */
#define MQ135_VC_MV            5000U    /**< 模块供电（VCC） mV */
#define MQ135_VREF_MV          3300U    /**< MCU ADC 参考电压 mV（VDDA/VREF+，SB12 默认接 VDDA） */
#define MQ135_ADC_MAX          4095U    /**< 12 bit ADC 满量程 */
#define MQ135_OVERSAMPLE       16U      /**< 采样平均次数 */

/** ADC 引脚上看到的电压（分压后），用于判断接线是否正常 */
#define MQ135_VPIN_EXPECT_MIN_MV  50U
#define MQ135_VPIN_EXPECT_MAX_MV  3100U

/** 一次采样结果（字段按 4 字节对齐排列，避免结构体填充字节） */
typedef struct
{
    uint32_t v_pin_mv;     /**< ADC 引脚电压 mV（分压后） */
    uint32_t v_ao_mv;      /**< 传感器 AO 电压 mV（分压前，已乘回分压比） */
    uint32_t rs_ohm;       /**< 估算的传感器电阻 Rs（Ω） */
    uint32_t r0_ohm;       /**< 当前使用的 R0（Ω） */
    int32_t  rs_r0_x100;   /**< Rs/R0 ×100（越大=空气越差；需先标定 R0 才有意义） */
    uint32_t adc_raw;      /**< 16 次平均后的原始码 0..4095（放最后，省填充字节） */
} MQ135_SampleTypeDef;

/** 相对等级（只用于阈值判断/演示，不代表标准空气质量指数） */
typedef enum
{
    MQ135_LEVEL_GOOD = 0,      /**< Rs/R0 高：空气较洁净 */
    MQ135_LEVEL_MODERATE = 1,  /**< 一般 */
    MQ135_LEVEL_POOR = 2,      /**< 较差 */
    MQ135_LEVEL_BAD = 3        /**< 差（可触发报警/开窗等策略） */
} MQ135_LevelTypeDef;

/**
 * @brief  初始化（ADC1 已由 CubeMX 的 MX_ADC1_Init 初始化，这里只加载默认 R0）
 * @note   R0 默认给 10 kΩ（常见模块的洁净空气量级），调用 MQ135_CalibrateR0() 可覆盖
 */
void MQ135_Init(void);

/**
 * @brief  采集一次（16 次平均），返回原始码、电压、Rs、Rs/R0
 * @param  out 结果指针
 * @note   阻塞约 1~2 ms（16 次采样）
 */
void MQ135_Read(MQ135_SampleTypeDef *out);

/**
 * @brief  用电网/教室“洁净空气”的当前读数标定 R0（建议预热 ≥3 分钟后调用）
 * @return 标定得到的 R0（Ω）；采样异常时返回 0 且不改变内部 R0
 */
uint32_t MQ135_CalibrateR0(void);

/** @brief 读取当前 R0（Ω） */
uint32_t MQ135_GetR0(void);

/** @brief 手动设置 R0（Ω），0 会被忽略 */
void MQ135_SetR0(uint32_t r0_ohm);

/**
 * @brief  按 Rs/R0 给出 4 级相对等级
 * @param  rs_r0_x100 Rs/R0 ×100（MQ135_Read 的输出）
 */
MQ135_LevelTypeDef MQ135_LevelFromRatio(int32_t rs_r0_x100);

#endif /* APP_MQ135_H */
