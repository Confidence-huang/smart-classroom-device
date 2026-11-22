# TB6612 A 通道风扇调速

## 2026-11-21 当前工程复核

2026-11-21 用户报告接线完成并明确要求烧录后，通过 ST-LINK `066BFF535153755087071921` / SWD 4000 kHz 将本轮归档 HEX 写入 `0x08000000`。CubeProgrammer 2.22.0 返回 `Download verified successfully`，随后软件复位成功，退出码 0；目标 ID `0x463`，电压 3.25 V。记录见 [flash.log](tb6612-evidence/2026-11-21/flash.log)。未发送 F1～F4，尚未观察风扇默认停止及各档动作。下文“本轮未烧录”为构建阶段记录，已由本次烧录更新。

本轮沿用已写好的 `App/Src/app_fan.c` 和 `app_led.c` 串口命令入口，核对 `.ioc`、GPIO 默认低电平、TIM3 初始化及 Keil 源文件引用，未覆盖后续加入的 USART2、视觉和显示代码。当前启动标识仍为 `uart-env-lcd-20261120.3`；本次产物为该工程的 2026-11-21 重构建，不能按历史 TB6612 单模块镜像认定其硬件验收。

新增可重复执行的构建和离线测试入口，工作目录为总控工作区：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File firmware/stm32/tests/build_fan.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File firmware/stm32/tests/test_fan.ps1
```

实际 Keil 全量构建：**0 错误、427 告警**。告警最多的是 `-Wextra-semi-stmt`（228）与 `-Wpadded`（127），尚未逐条清理。实际主机测试通过默认待机、四档占空比、方向、非法档位不改输出、STBY 优先关闭及启动失败路径。此结果不代表硬件动作通过。本轮未烧录、未连接或启动电机。

[本轮 HEX](tb6612-evidence/2026-11-21/EcoVision_F413ZH-fan-rebuild.hex) · [构建日志](tb6612-evidence/2026-11-21/keil.log) · [主机测试](tb6612-evidence/2026-11-21/host.log) · [哈希](tb6612-evidence/2026-11-21/sha256.txt)。HEX SHA-256：`D0EEFB53E15CD0DA8F9600D24DA7F44906369D1E01149818CD86E8ABCBDE8276`。

待补的最少硬件信息：确认电机为两线有刷直流电机，提供额定／启动或堵转电流，并确认外部 5 V 电源的供电能力。若是内部自带驱动的无刷风扇，需先按风扇规格重新判断供电 PWM 方式。连接前 VM 保持断开；逻辑线核对和上电 F0 检查完成后，再进入电机动作验证。为防 MCU 复位时 GPIO 高阻造成悬空，建议 STBY 与 GND 接 10 kΩ 下拉；先检查模块是否已有该下拉。

2026-11-18，固件 tb6612-20261118.1。用户提供模块背面照片可读到两列端子，左列自上而下为 PWMA、AIN2、AIN1、STBY、BIN1、BIN2、PWMB、GND，右列为 VM、VCC、GND、AO1、AO2、BO2、BO1、GND。正面芯片文字在照片中无法独立辨读，按用户指定 TB6612 模块及端子布局设计，芯片型号仍可通过近照／采购资料补核。

## 接线

全部断电接线，NUCLEO 继续由 CN1 USB 供电，电机电源为用户确认的独立稳压 5 V。控制侧逻辑电源 VCC 使用 3.3 V，所有 GND 共地。

| TB6612 模块 | 对应连接 |
| --- | --- |
| VM | 独立电机电源 +5 V |
| VCC | NUCLEO 内侧 CN8-7（3V3） |
| GND | 独立电源负极及 NUCLEO 内侧 CN10-27 |
| PWMA | PB5／内侧 CN7-13（D22），TIM3_CH2，AF2 |
| AIN1 | PB10／内侧 CN10-32（D36），GPIO |
| AIN2 | PE0／内侧 CN10-33（D34），GPIO |
| STBY | PF15／内侧 CN10-12（D2），GPIO |
| AO1、AO2 | 接两线 5 V 电机的两端 |
| BIN1、BIN2、PWMB | B 通道不用，接 GND，BO1、BO2 留空 |

电机两端接驱动输出 AO1/AO2。若转向反了，断电后交换两根电机线。TB6612 是全桥驱动，按厂家电路处理供电去耦和电机噪声，旧继电器接法的单个电机并联续流二极管方案不直接搬到全桥输出。启动和堵转电流还需核对，小板实际散热决定连续负载能力。 [东芝 TB6612FNG 手册](https://toshiba.semicon-storage.com/info/TB6612FNG_datasheet_en_20141001.pdf?did=10660&prodName=TB6612FNG) · [ST 内侧排针表 16](https://www.st.com/resource/en/user_manual/um1974-stm32-nucleo144-boards-mb1137-stmicroelectronics.pdf#page=50)

## 命令与行为

COM4（以电脑实际枚举为准），115200/8N1，无流控，文本／ASCII 模式。灯环仍用单字符 0～4 调暖光亮度。电机命令使用：

| 命令 | PWMA 占空比 | 控制状态 |
| --- | --- | --- |
| F0 | 0% | STBY=0，AIN1=0、AIN2=0，输出高阻，电机自然停转 |
| F1 | 40% | STBY=1，AIN1=1、AIN2=0 |
| F2 | 60% | 同上 |
| F3 | 80% | 同上 |
| F4 | 100% | 同上 |

频率 20 kHz。TIM3 输入时钟 96 MHz，PSC=0、ARR=4799，CCR2 分别为 0、1920、2880、3840、4800。与灯环的 TIM2/800 kHz 位流使用不同定时器。TIM3 不使用 IRQ 或 DMA，保留 K230 的 USART2、EC200U 的 USART6 和日志 USART3。

上电时 STBY 和方向脚先初始化为低，PWMA 初值为 0，再启动 PWM。串口两字符命令超时 500 ms，错误命令不改变电机输出。回执如 `[FAN] OK level=2 duty=60%` 代表已配置 GPIO/PWM。没有转速反馈，档位对应的是 PWM 占空比。各档实际转速、能否可靠起转需要台架测量。若低档转不起来，先发 F0，再在固定好电机后短暂测试较高档，按实际结果调整最低档。

## 分阶段验证

先核对控制线并保持 VM 断电，烧录后测 STBY、AIN1/2 和 PWMA，确认上电为 F0。再接好电机并固定扇叶，给 VM 供电，从短时命令测试开始，记录电源电压、电流、实际转向、各档起转及 F0 停转。复位时确认输出关闭。板端 PWM 配置和电机实物动作分别验收。

## 当前验证

CubeMX 6.15.0 在副本中生成并核对 TIM3 20 kHz、PB5 AF2 和三个初始低电平 GPIO，检查差异后合入现有工程。Keil AC6 6.16 全量构建为 **0 错误、385 告警**，HEX 已归档。同一份 app_fan.c 经主机 HAL 桩测试通过默认待机、四档 CCR2、固定方向、非法档位不改输出、STBY 优先关闭和 PWM 启动失败路径。模拟 HAL 测试属于离线验证。尚未烧录此版本，也尚未确认模块实际电机动作与档位。

[TB6612 固件 HEX](tb6612-evidence/EcoVision_F413ZH-tb6612-20261118.1.hex) · [构建日志](tb6612-evidence/keil.log) · [CubeMX 记录](tb6612-evidence/cubemx.log) · [离线测试](tb6612-evidence/host.log) · [SHA-256](tb6612-evidence/sha256.txt)
