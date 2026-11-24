# DMA优化候选资源（2026-11-24）

USART2 RX=DMA1 Stream5 Channel4；USART6 RX=DMA2 Stream1 Channel5；512字节循环，HT/TC/IDLE、IRQ6。灯环DMA1 Stream7保持。IWDG无外部引脚，由单一任务健康监督者在启动宽限后启用，不在CubeMX自动初始化。接线不变，硬件未测；详情与候选见 [优化与上板清单](OPTIMIZATION-20261124.md)。

# 本轮模块接线入口（2026-11-23）

以 [MODULE-WIRING.md](MODULE-WIRING.md) 为本PR固定资源与队友接线入口。以下旧台架记录保留历史含义，串口五档命令不代表当前main支持；当前AUTO规则见新表。红外改用PE11/TIM1与PA2/TIM5，TIM4包络，旧TIM1舵机候选有冲突，暂不接。

# EcoVision STM32 外设资源表初稿

## 2026-11-21 上报修复后的当前部署（优先于以下历史表）

当前实际固件为net-20261121.8，USART6 PG14/PG9 AF8已启用并通过主UART真实AT/TLS/MQTT上报，不能再视为“仅预留”。IRQ优先级6、RX环2048字节、独立4KB网络任务栈；FreeRTOS堆改为24576字节。链接器预留0x08160000起最后128KB作私有设备配置，后续烧录/链接不要覆盖或整片公开导出。

真实温湿度与ADC已恢复有效；BH1750驱动已随本版本烧录，但本次未取得有效lux。当前K230仍待正式入口运行，人数保持未知。输出默认关闭和原PC本地命令保留，网络状态已接LCD。详细代码、证据、操作及未验收项见[NET-UPLINK.md](NET-UPLINK.md)。下面“USART6未启用”“当前C固件无上报”等为修复前记录。

## 2026-11-21 接线与占用更新（优先于下方历史候选）

干簧管新增占用：**PE13 / D3 / 内侧 CN10-10**，普通 GPIO 输入、内部上拉，DO 轮询消抖，无 EXTI/DMA，不启用该脚的 TIM1_CH3。模块 VCC=CN8-7 3.3 V、GND=CN10-27。用户提供的三针 LM393 模块说明书支持 3.3～5 V，预期有磁为低、无磁为高，尚待实测。该配置已进 `.ioc` 与固件，后续舵机选脚避让 PE13。详见 [干簧管台架记录](REED-BENCH.md)。

最新实测补充：reed-20261121.1 已烧录校验；用户提供串口日志包含四组 REED 低→高转换，输入变化检测已响应。尚未同步确认磁铁位置与窗口端点，不改变 UNKNOWN 标定状态。

光照占用补充：当前集成工程的光敏 ADC 实际运行使用 **PC5 / ADC1_IN15**，由环境任务独占 ADC1，现封装为 app_light.c/h；初始 .ioc 的 PA3/IN3 仍是旧配置，业务运行时改配通道15。PC5 应视为已占用，不能按旧“未采样”候选表再分配。新版封装未烧录，见 [光照记录](LIGHT-BENCH.md)。

用户已选择独立 **GY-302/BH1750**：复用 **PB8 / D15 / CN7-2 SCL、PB9 / D14 / CN7-4 SDA，I2C1 AF4 100kHz**，与AHT20共总线，0x23/0x5C与0x38不冲突。BH1750驱动、内部快照和LCD lux显示已构建/离线测试，未烧录、未接线实测。实物电源入口、稳压器及上拉电压待核，禁止只据芯片规格直接确定载板VCC接法。详见 [BH1750台架记录](BH1750-BENCH.md)。

实物照片补充：已确认GY-302 V322及五端子丝印，正面有稳压器，电气参数尚未测。首轮低压试接VCC=CN8-7 3V3、GND=CN10-27、SCL/SDA如上，ADDR保留板载默认状态；3.3V输入能否满足板载稳压器及芯片供电仍以实际电压和通信验证为准。

用户要求仅使用内侧已焊排母；用户已报告 TB6612 接线完成，后续分配引脚须避让下表。接线完成是用户报告，未通过照片或仪表独立复核。

| 模块 | 已分配资源 | 当前证据 |
| --- | --- | --- |
| TB6612 A 通道 | PWMA=PB5 / CN7-13 / D22 / TIM3_CH2 AF2；AIN1=PB10 / CN10-32 / D36；AIN2=PE0 / CN10-33 / D34；STBY=PF15 / CN10-12 / D2 | 当前 `.ioc` 与驱动一致；已烧录、校验；COM4 收到 `[FAN] OK level=0 duty=0%`；起转、调档、停止动作仍待观察 |
| TB6612 供电与电机 | VM=外部稳压 5 V；VCC=CN8-7 3.3 V；GND=CN10-27 并与外部电源负极共地；电机接 AO1/AO2；BIN1/BIN2/PWMB 接地，BO1/BO2 留空 | 用户按所给方案接线；电机类型、启动电流、实际端电压待核 |
| 灯环 | PB11 / CN10-34 / D35 / TIM2_CH4 AF1；DMA1 Stream7 Channel3 | 已有点亮记录；与 TIM3 分开，保留五档暖色命令 0～4 |
| PC 调试控制 | PD8/PD9 USART3 AF7，ST-LINK VCP COM4，115200/8N1；USART3 IRQ | 2026-11-21 直接打开 COM4 并发送 F0 得到成功回执；串口已释放 |
| K230 | PD5/PD6 USART2 AF7，内侧 CN9-6 TX / CN9-4 RX | 当前 `.ioc` 已启用，不能再作为空闲口；本轮未重验视觉通信 |
| EC200U | PG14/PG9 USART6 AF8，CN10-14 / CN10-16 | 继续预留，当前 `.ioc` 未启用 USART6 |
| 红外 | PA0/PA1 UART4 AF8 | 已配置，PA0 的内侧引出为 CN10-29；PA1 内侧可达引出须另核，不沿用外侧 CN11 接线要求 |
| 其他已用资源 | PB8/PB9 I2C1；PA3 ADC1_IN3；PC13 B1；PB0/PB7/PB14 板载 LED；PA13/PA14 SWD；PH0/PH1 时钟；TIM6 HAL 时基与 SysTick RTOS | 以当前 `.ioc` 核对；业务代码动态配置的 PB11/TIM2 也必须保留 |

风扇档位 F0～F4 = 0/40/60/80/100%，TIM3 输入 96 MHz，PSC=0、ARR=4799，20 kHz，无 TIM3 IRQ/DMA。回执仅证明驱动已设置输出，不证明电机动作。下一阶段先补齐风扇台架验证，公共 MQTT 字段与主题保持现状。接线详见 [TB6612-FAN.md](TB6612-FAN.md)，实测记录见 [验收表](FAN-BENCH-20261121.md)。

2026-11-17，NUCLEO-F413ZH，接口基线 1.0.0。主方案按 UniKnect Gen1 Pro QADP V1.1 叠插。载板丝印版本、跳帽及外设接线还需实物对照。表中 MCU 复用依据是 CubeMX 的 STM32F413 GPIO 数据库，排针依据是 UM1974 Rev 11 表 16、21 及本目录联调指南。

| 功能 | 状态 | 排针／MCU／外设 | IRQ／DMA | 电平、电源及待核 |
|---|---|---|---|---|
| 调试／烧录 | SWD 实测识别 | CN1 ST-LINK → PA13 SWDIO、PA14 SWCLK，专用 SWD 复用 | 无 | 目标电压 3.24 V，ST-LINK V2J46M32；CN4、JP3、JP5 实物位置待核 |
| PC 日志 | 工程已配置，COM4 已枚举 | CN1 VCP，PD8 TX／PD9 RX，USART3 AF7，115200 8N1 | 当前轮询，无 DMA | 板载 3.3 V 侧。核对 SB5/SB6 等焊桥；PD9 避免外部 TX 与 ST-LINK TX 共驱动 |
| K230 双向 UART | 叠插候选 | CN11-41／43（也在 CN9-6／4），PD5 TX／PD6 RX，USART2 AF7，115200 8N1 | 候选 USART2_IRQn，RX 环缓冲 ≥512 字节，第一版先用 IRQ，DMA 未分配 | K230 的 FPIOA、排针和电平待确认，独立供电，共地 |
| EC200U AT UART | 叠插候选，USB AT 已枚举 | CN10-14 D1／CN10-16 D0，PG14 TX／PG9 RX，USART6 AF8；J308 1–3、2–4 经电平转换接主 AT 口 | 候选 USART6_IRQn，DMA 未分配 | J401 USB AT 为 COM10。J308 和 STM32 侧波特率、流控待核。按载板规定供电，配套 5 V 输入和 LTE 天线。J307 原始模组侧电平另查 |
| 板载温湿度 | 现有 I²C1 已配置 | CN7-2 D15／CN7-4 D14，PB8 SCL／PB9 SDA，AF4 开漏，100 kHz，地址 0x38 | 当前阻塞 HAL，无 IRQ/DMA | 3.3 V 总线，上拉与 S502 ARDU/MCU 档待核。AHT20-F 的 C/HAL 采样待实测 |
| B1 实体按键 | PC13 已配置输入 | NUCLEO B1／PC13，GPIO 输入，原工程开内部上拉 | 轮询候选，EXTI13 未启用 | 3.3 V MCU。按下与释放电平以实测决定，30 ms 消抖，≥2 s 长按，本轮无按键行为 |
| 系统时基 | FreeRTOS 工程已分配 | SysTick 供 FreeRTOS，TIM6 供 HAL 毫秒计时，无外部引脚 | SysTick 和 TIM6_DAC_IRQn，无 DMA | 两个 tick 均为 1 kHz。TIM6 已占用 |
| LED | 16 灯 WS281x 已点亮，串口五档控制已进板测试 | 内侧 CN10-34／D35，PB11 AF1／TIM2_CH4；内侧 CN10-27 可接 GND。LD2／PB7 继续作板载心跳 | TIM2 更新事件，DMA1 Stream7 Channel 3；USART3 IRQ 接收调试命令 | 灯环独立 5 V、共地，DI 经 3.3 V→5 V 缓冲。上电发全黑帧。详见 [五档控制](LED-SERIAL.md) |
| 5 V 风扇 | 按实物端子布局实现 TB6612 A 通道，已编译，待接线实测 | PWMA=PB5／CN7-13／TIM3_CH2 AF2；AIN1=PB10／CN10-32；AIN2=PE0／CN10-33；STBY=PF15／CN10-12。均为内侧排母 | TIM3 硬件 PWM 20 kHz，无 IRQ/DMA，与灯环 TIM2 分开 | VM=独立 5 V、VCC=3.3 V、共地，电机接 AO1/AO2。启动／堵转电流待测。详见 [TB6612 接线](TB6612-FAN.md) |
| 舵机 | 已发现冲突 | 旧 PE9／CN10-4 D6 是 TIM1_CH1 AF1，也是 QADP 模组 PWRKEY 控制脚。叠插时从其他可用引脚为 TIM1 选通道 | PWM 无 IRQ/DMA | 舵机独立低压电源、共地、信号阈值及实际行程待测。50 Hz 为候选，禁止沿用旧扫角行程作窗户端点 |
| 干簧管 | 待确认 | 空闲 GPIO 轮询，排针和 MCU 脚待选 | 先轮询 | 查常开／常闭、实际关闭电平、位置、拉电阻；单闭窗反馈仅区分「关闭到位」和「未确认关闭」 |
| YS-IRTM V6.08 红外收发 | 实物丝印已确认，UART4 已配置，待接线 | UART4 AF8，TX **PA0／CN11-28**，RX **PA1／CN11-30**。9600/8N1，无流控。PA0 也引出到 CN10-29（D32） | UART4_IRQn 优先级 6，无 DMA | 模块按 5 V 供电、5 V TTL 处理；与 STM32 之间双向电平转换、共地。PA1 内部上拉。模块负责 38 kHz NEC 收发，不占用 STM32 定时器。见 [红外台架记录](IR-YS-IRTM.md) |
| 原工程 MQ-135 | 保留代码，本阶段不采样 | CN9-1 A0，PA3／ADC1_IN3 | 原工程轮询 | 5 V 模块的 AO 需分压方可接 3.3 V ADC。本期不把原始 ADC 当 CO₂ 或精确浓度 |
| 载板光敏、LCD、模块控制 | 叠插避让 | 光敏 PC5；LCD 用 SPI1，DC=PF12、CS=PD14，SPI1 的具体走线待核；D4、D6、D7、D9 见载板指南 | 本阶段无 | D4 DC_EN、D6 PWRKEY、D7 传感器中断、D9 模组复位。先按实物核对贴装和排针，再分配负载引脚 |

独立 AT 载板走跳线时，USART2 可改给 EC200U、USART6 可改给 K230，前提是核对所用载板接口和电平。叠插时 D0/D1 已用于 EC200U。USART2、USART6 和 USART3 的候选脚在 MCU 上彼此独立。USART2 和 USART6 当前未在 .ioc 启用，实际收发和负载时延需分开测。

现有时钟为 HSE bypass 8 MHz，SYSCLK 96 MHz、PCLK1 48 MHz、PCLK2 96 MHz，APB1/APB2 定时器输入均为 96 MHz。不同通道共享同一个定时器的 PSC 和 ARR。TB6612 采用 TIM3，PSC=0、ARR=4799，20 kHz。灯环 TIM2 独立运行。TIM1 保留给候选 50 Hz 舵机，PSC=95、ARR=19999。TIM6 是 HAL 时基。复位到 GPIO 初始化之间的外部关断由驱动电路的偏置保证并现场测量。

CubeMX 数据库列出的 DMA 备选：USART2 RX/TX 可用 DMA1 Stream5/6 Channel 4，USART6 RX 可用 DMA2 Stream1 或 2 Channel 5、TX 可用 Stream6 或 7 Channel 5。两条 UART 的候选分处 DMA1、DMA2。确需启用时，再核对同一控制器内的其他传输占用和 IRQ 优先级。
