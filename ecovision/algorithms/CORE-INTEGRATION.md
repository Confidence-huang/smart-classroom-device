# EcoVision 核心算法接入

2026-11-22。`ev_core_step` 是算法入口。共享工作区 STM32 工程已接入 50 ms `controlTask`。环境信号经 `ev_stm32_inputs` 适配，K230 则把每个已接纳的帧按顺序交给控制任务。本次接入的任务和设备行为见 [FreeRTOS 集成记录](../firmware/stm32/CONTROL-RTOS.md)。

## 接收参数

| 模块 | 参数 | 提交规则 |
|---|---|---|
| AHT20 | 温度 ×100℃、湿度 ×100%RH | 使用环境快照 `temp_x100`、`humi_x100`、`th_valid`、`sampled_at` |
| PC5 光敏 | ADC 原码 0–65535 | 仅诊断和相对变化，保留三点中值 |
| BH1750 | 照度 ×100 lx | 使用 `lux_x100`、`lux_valid`、`light_error`。摆放位置和工作面参考值待实测 |
| K230 | 0–20 人、有效性、序号和会话 | 复用 UART 校验。快照适配器检测重复序号与会话变化。正式连续帧确认建议按顺序提交各个已接纳的帧，轮询快照可能跳过同一周期内的多个帧 |
| CO₂ | `co2_ppm`、采样时刻、有效性、来源、错误 | 驱动获得有效新样本时填入，未接入时保持全零。量程及暖机由对应设备驱动约束。算法当前最多接受 10 秒年龄 |
| 室外与设备反馈 | `outdoor_open_permitted`、窗开/闭确认、空调停机/制冷确认、`daylight` | 由相关模块检查雨、风、室外空气、权限、限位和反馈的时效后传入。缺失或过期清零，互相矛盾的开/闭以及停机/制冷确认按未知处理 |
| 执行与模式 | `ready_mask`、当前灯扇状态、AUTO/REMOTE/LOCAL_OFF | 相应设备安装、许可、空闲、能够朝建议方向动作并满足间隔时才可用。灯帘动作后等待新鲜照度并重新评估。外部仲裁器负责设备保护、实际输出和手动租期 |

温湿度、光敏原码、BH1750 与 CO₂ 各自滤波。有效新样本才进窗口，重复快照不刷新年龄。当前环境快照共用一枚发布时间，同轮 BH1750、AHT20、PC5 实际采样时间存在偏差。精确到各传感器的时间需采集任务分别提供。对于无效视觉帧，当前快照只保留最后**有效**帧的时间，适配层记录观察到该无效帧的本机时间，不改最后有效采样时间。

## 其他调用方使用快照时的示例

在唯一控制任务中初始化静态 `EvCoreState` 与 `EvStm32Inputs`。周期建议 ≤100 ms。以下片段的 `env_published` 由集成方在环境任务首次发布快照后置位。其他字段在对应模块接入后填入。

```c
static EvCoreState core;
static EvStm32Inputs adapter;
/* 任务启动时调用一次 ev_core_init(&core)。 */

EnvironmentSnapshot env;
EvVision vision;
EvCoreInput input = {0};
EvCoreOutput output;
AppEnvironment_GetSnapshot(&env);
AppVision_GetSnapshot(&vision);
input.base.now_ms = HAL_GetTick();
input.base.mode = current_mode;
input.base.fan_actual = actual_fan_on;
input.base.led_actual = actual_led_on;
ev_stm32_inputs(&adapter, &input, env_published ? &env : NULL, &vision);
/* 在这里填本轮的 CO₂ 新样本、许可和执行器反馈。 */
ev_core_step(&core, &input, &output);
```

`score[]` 是温度、湿度、通风、照度四项。权重为 35/15/30/20，最低分上限为最差项加 20。`total_score=255` 表示总分未知，`coverage_pct` 表示有效分项的权重之和。`measured_score` 可用于显示部分分项评价。`impact_x100[]` 是即时受影响人数的百分之一单位，若需等效人分钟，按有效时长累计。

`output.base` 是现有灯扇规则的输出。`output.advice[]` 是五设备建议，默认 `EV_HOLD`。集成方应为每台设备选定唯一控制入口，避免把两路建议分别写入同一驱动。调档和窗帘请求应由执行器按当前档位、到位和等待状态去重。`simulation=1` 时只允许离线消费输出。所有机械和空调建议都需驱动再次核对执行条件。

已实现评分、数据覆盖率、人数影响、通风迟滞、照度持续确认、制冷持续确认、窗口和空调互锁、窗帘与灯光的建议选择。完整现场策略还依赖设备超时处理、相邻档效果验证、室外条件判定、反馈时效和驱动冷却间隔。当前演示阈值沿用设计文档候选值，尚未实测定参。
