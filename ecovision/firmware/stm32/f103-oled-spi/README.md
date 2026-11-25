# F103 模拟空调 OLED

F103 接收项目的 24 位红外演示帧。上电和关机时 OLED 显示 `OFF`，开机后居中显示设定温度（16–30°C）。F413 通过 UART5 控制 ZJ-IR-V2 发射，F103 通过红外接收头收信号。控制台显示的学习模块应答与 F103 屏幕由各自的链路产生。

## 接线

### F413 发射端（UART5，CN8 内排）

| F413 | ZJ-IR-V2 |
| --- | --- |
| CN8-10（PC12 / D47 / UART5_TX） | RXD |
| CN8-12（PD2 / D48 / UART5_RX） | TXD |
| CN8-7（3.3 V） | VCC |
| CN8-11（GND） | GND |

### F103 接收端与 OLED

| 模块 | F103 |
| --- | --- |
| OLED SCL / SDA | PB6 / PB7（软件 SPI） |
| OLED RES / DC | PB8 / PB9 |
| 红外接收头 OUT | PA0（TIM2_CH1） |
| OLED 与接收头供电 | 兼容的 3.3 V 电源 |
| OLED 与接收头的 GND | F103 GND |

发射端与接收端通过红外光通信。F103 使用内部 HSI 8 MHz，SWD 继续使用 PA13/PA14。

## 构建和验收

运行 `python build.py --toolchain <Arm Compiler 6 bin>`，构建需要 STM32Cube F1 V1.8.7 的 CMSIS。输出 `build/f103-ir-ssd1306.hex`、`build/f103-ir-sh1106.hex` 和 `build/images.json`。实板验收使用 SSD1306 候选，OLED 开机、调温和关机由用户确认通过。先识别 F103（实板读回 ID 0x410、64 KiB Flash），再用 SWD 对应用 HEX 进行编程校验。复位后核对 OLED 上的 `OFF → 21°C → 26°C → OFF`。

CI 在主机上从 SPI 信号核对 OLED 位图，并覆盖红外接收、计时回绕、校验和、参数范围和异常恢复。SWD 可以只读查看 `g_ir_debug`，数组依次包含标记、毫秒计数、捕获边沿数、有效帧数、坏帧数、捕获异常数、PA0 电平、电源状态、温度、模式、风速和最后命令/参数。调试符号地址以相应的构建 map 为准。

接收端由设备仓 PR #10 的 F103 实现演进，显示精简代码来自 `27c326d`。
