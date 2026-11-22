/**
 * @file    app_servo.h
 * @brief   舵机 PWM 驱动（TIM1_CH1 / PE9，50 Hz，脉宽 500~2500 us）
 *
 * 接线（见 docs/01_硬件选型与接线.md）：
 *   舵机信号线（橙/白） -> CN10 pin4 (D6) -> PE9 = TIM1_CH1
 *   舵机 VCC（红）      -> 外部 5V 电源（不要从板子 +5V 取电，SG90 堵转可达 0.6~0.8A）
 *   舵机 GND（棕/黑）    -> 与 Nucleo 的 GND 共地（必须！否则抖动/乱转/串口乱码）
 *
 * 为什么这里直接写寄存器而不是用 HAL_TIM：
 *   CubeMX 工程里没有启用 TIM 驱动（HAL_TIM_MODULE_ENABLED 未打开，也没有 MX_TIM1_Init），
 *   本文件用最小寄存器配置把 TIM1 拉起来，好处是“重新生成代码也不会丢”，
 *   缺点是它绕开了 CubeMX 的配置。等熟悉后可在 CubeMX 里给 PE9 选 TIM1_CH1 →
 *   PWM Generation1 CH1，然后改用 HAL_TIM_PWM_Start()，两者功能等价。
 *
 * 量纲：脉宽 us。SG90/MG996R 的标准范围是 500~2500 us 对应 0~180°
 * （不同型号会略有差异，若到极限有异响/抖动，把范围收窄到 1000~2000 us 再试）。
 */
#ifndef APP_SERVO_H
#define APP_SERVO_H

#include <stdint.h>

#define SERVO_MIN_PULSE_US     500U    /**< 0° 对应脉宽 */
#define SERVO_MAX_PULSE_US     2500U   /**< 180° 对应脉宽 */
#define SERVO_DEFAULT_PULSE_US 1500U   /**< 上电中位 1.5 ms */
#define SERVO_PERIOD_US        20000U  /**< 50 Hz 周期 20 ms */

/**
 * @brief  初始化 PE9 为 TIM1_CH1 PWM 输出（50 Hz），并把脉宽置为中位 1.5 ms
 * @note   会打开 GPIOE、TIM1 时钟；必须在 HAL_Init() 之后调用
 */
void Servo_Init(void);

/**
 * @brief  设置脉宽（us），超出 500~2500 会被钳位
 */
void Servo_SetPulseUs(uint16_t pulse_us);

/** @brief 读取当前设定的脉宽（us） */
uint16_t Servo_GetPulseUs(void);

/**
 * @brief  设置角度（0~180°），线性映射到 500~2500 us
 * @param  deg 角度，>180 按 180 处理
 */
void Servo_SetAngleDeg(uint16_t deg);

#endif /* APP_SERVO_H */
