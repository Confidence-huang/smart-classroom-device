# 模块接线参考：NUCLEO-F413ZH

2026-11-25 从已关闭的 PR #5（`feat/stm32-modules-ir-wiring-20261123`，`d120970`）提取，按当前 main 的实现更新红外链路。只记录接线与资源分配，不代表下列模块全部通过实板验收。

仅使用已焊接的内侧排母，按 MCU 名、Arduino 丝印与 CN 编号三者核对；断电后插拔。

## 1 固定信号接线

| 模块端子 | STM32 / 丝印 / 孔位 | 配置与注意 |
| --- | --- | --- |
| WS281x 灯环 DI | PB11 / D35 / CN10-34 | TIM2_CH4 AF1，800 kHz，DMA1 Stream7 Channel3；DO 留空 |
| TB6612 PWMA | PB5 / D22 / CN7-13 | TIM3_CH2 AF2，20 kHz |
| TB6612 AIN1 | PB10 / D36 / CN10-32 | GPIO，固定正转方向 |
| TB6612 AIN2 | PE0 / D34 / CN10-33 | GPIO |
| TB6612 STBY | PF15 / D2 / CN10-12 | GPIO，上电低；不得常接高电平绕过关闭 |
| 干簧管 DO | PE13 / D3 / CN10-10 | GPIO 上拉，30 ms 消抖；不用该脚 TIM1_CH3 |
| BH1750 SCL | PB8 / D15 / CN7-2 | I2C1 AF4，开漏 100 kHz，与板载 AHT20（0x38）共总线 |
| BH1750 SDA | PB9 / D14 / CN7-4 | I2C1 AF4；ADDR 保持板载默认低地址 |
| 红外链路 | 见 `f103-oled-spi/README.md` | F413 UART5 → ZJ-IR-V2 → 红外光 → F103 PA0 |

对应实现：`App/Src/app_led.c`（WS281x G,R,B 顺序）、`app_fan.c`（TB6612）、`app_reed.c`、`app_bh1750.c`、`app_environment.c`、`f103-oled-spi/`。

## 2 红外链路（当前方案）

| F413（CN8 内排）/ 外部电源 | ZJ-IR-V2 |
| --- | --- |
| CN8-10（PC12 / D47 / UART5_TX） | RXD |
| CN8-12（PD2 / D48 / UART5_RX） | TXD |
| 独立电源模块（不与 Nucleo 3V3 并联） | VCC |
| 公共 GND（与 Nucleo / 电源模块共地） | GND |

| 模块 | F103 |
| --- | --- |
| OLED SCL / SDA | PB6 / PB7（软件 SPI） |
| OLED RES / DC | PB8 / PB9 |
| 接收头 OUT | PA0（TIM2_CH1） |
| SWD | PA13 / PA14 |

旧的 PE11 发射板 + PA2 接收头方案已停用，不再使用 TIM1 产生 38 kHz 载波，也不再占用 PA2 捕获。F413 只通过 UART5 让模块学习/回放，实际光学发射由模块完成；F103 只听红外光。

## 3 电源与共地

- 公共地：CN10-27 GND、外部稳压负极、各模块 GND。
- 逻辑 3.3 V：CN8-7，可给低功耗逻辑；电机与灯环不得由 NUCLEO 供电。
- 灯环：外部稳压 5 V → 5V/VCC；DI 建议经 3.3→5 V 缓冲并串约 330 Ω，5 V 不得进 PB11。
- TB6612：VM→外部 5 V，VCC→3.3 V，AO1/AO2→电机，未用 BIN1/BIN2/PWMB 接地。
- 干簧管：VCC 3.3 V，DO→PE13。
- GY-302（BH1750）：先用单一 3.3 V 低压试供；SCL/SDA 空闲高电平须在 3.3 V 逻辑范围，不应答不要直接改 5 V。
- 外部 3.3 V 正极不与 USB 供电的 NUCLEO 3V3 并联，只共地；每孔只插一根线，用端子分线。
- 红外模块（`ZJ-IR-V2`）：2026-11-28 实板改由独立电源模块供电并与 Nucleo 共地（`UART5` 信号线 `PC12`/`PD2` 不变），外部 3.3 V 正极不与 Nucleo `3V3` 并联；不从 GPIO 直接驱动大功率红外 LED。

## 4 定时器与串口分配

| 资源 | 用途 |
| --- | --- |
| TIM2 | 灯环 800 kHz（CH4） |
| TIM3 | 风扇 20 kHz（CH2） |
| TIM4 | 1 MHz 包络时基，无输出脚 |
| TIM5 | 1 MHz 接收时基（旧红外方案；当前未用于 PA2） |
| TIM6 | HAL 时基 |
| USART2 PD5/PD6 | K230，CN9-6 / CN9-4，3.3 V，交叉接线，双向 |
| USART6 PG14/PG9 | EC200U，CN10-14 / CN10-16，保持现有扩展板路径，不改 AT 跳帽 |
| USART3 PD8/PD9 | ST-LINK 日志与调试，115200/8N1 |
| UART5 PC12/PD2 | ZJ-IR-V2（见第 2 节） |
| SPI1 PA5/PA7 + PF12 DC + PD14 CS | LCD |

TIM1 不再被红外占用，理论上可另作他用；若要接 50 Hz 舵机（PE9/TIM1_CH1），必须先核对 `.ioc`、MSP 初始化与现有 PWM 输出，不能直接套用旧方案。

## 5 未确认项

- 干簧管闭窗有效电平未校准：单传感器只能区分“关闭到位/未确认关闭”，不能确认全开。
- BH1750 地址可能为 0x23 或 0x5C，需按实物读回；lux 观测见 PR #15。
- 外置温湿度型号未确认（SHT3x 0x44/0x45、SHT20 0x40 命令不同），未接电前不写驱动。
- 灯环 3.3→5 V 缓冲、TB6612 启动/堵转电流、GY-302 板载稳压压差均未实测。
- 舵机、CO2、窗帘等未接入。

## 6 证据边界

本文件由 PR #5 的 `MODULE-WIRING.md` 整理而来，删去已废弃的 PE11/PA2 方案，补入当前 UART5 链路。灯环、风扇的历史验收属于 D03/W02 批次；干簧管与 BH1750 的上板观测分别见 Issue/PR 记录。缺少观测的项目按“未测”处理，不写通过。
