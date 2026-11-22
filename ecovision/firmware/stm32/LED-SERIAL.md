# 灯环串口五档亮度

当前固件版本 led5-warm-20261118.2。使用 16 颗 WS281x 灯环，16 颗同时显示淡黄色暖光。RGB 配比为 4:3:1，是颜色调配，尚无色温、显色指数、频闪或照度测量，实际观感由现场确认。

ST-LINK 虚拟串口设为 **115200、8 数据位、无校验、1 停止位、无流控**，本机当前端口 COM4。串口助手选文本／ASCII 发送，发送一个字符 0～4，可附 CR/LF。HEX 发送模式下，对应字节是 30～34。

| 字符 | 档位 | 各灯珠 R、G、B 数值 |
| --- | --- | --- |
| 0 | 全灭 | 0、0、0 |
| 1 | 低亮 | 8、6、2 |
| 2 | 中低亮 | 24、18、6 |
| 3 | 中亮 | 48、36、12 |
| 4 | 高亮 | 96、72、24 |

档位按首轮台架电流选择，4 档的红通道约为满码的 38%。上电发全黑帧。命令执行并完成 DMA 传输后返回例如 `[LED] OK level=2 RGB=24,18,6`。这表示 MCU 已发帧，灯环亮度由现场观察确认。无效字符返回错误，空白和换行忽略。UART 错误或接收环缓冲溢出时丢弃待处理输入并记录错误，当前输出保持。LED 任务替换当前的红外台架任务，后续整机控制由公共优先级契约统一接入。

接线采用已点亮的内侧排母方案，PB11 为 **CN10-34（D35）**，GND 可用 **CN10-27**。DI 经 3.3 V→5 V 数据缓冲接 PB11，DO 留空，灯环独立 5 V 供电并共地。

实现位于 `EcoVision_F413ZH/App/Src/app_led.c`，USART3 IRQ 缓存输入，低优先级 FreeRTOS 任务处理命令。TIM2_CH4 的 800 kHz 位流由 DMA1 Stream7 Channel 3 输出，单次传输小于 1 ms，故障超时 5 ms。TIM2/PB11/DMA 由驱动初始化，CubeMX 配置负责 USART3 IRQ 和任务，业务位于独立文件和 USER CODE 区。CubeMX 重生成副本合入时保留 Keil App 分组和 AC6 设置。用 [原构建命令](PHASE1-RESULTS.md#构建与软件测试) 重建现有 Keil 工程即可生成该固件。

[暖光固件 HEX](led5-warm-evidence/EcoVision_F413ZH-led5-warm-20261118.2.hex) · [构建日志](led5-warm-evidence/keil.log) · [SHA-256](led5-warm-evidence/sha256.txt)

暖光版实测：Keil AC6 全量构建 0 错误、379 告警，烧录校验成功。COM4 对 0～4 五档均返回正确 RGB，SWD 逐档验证完整的 16 灯 GRB 位流及全零复位尾部。测试结束发送 0 档并释放 COM4。实际暖色观感待现场确认。 [五档日志](led5-warm-evidence/levels.log) · [烧录日志](led5-warm-evidence/flash.log)

2026-11-18 实测记录：Keil AC6 6.16 全量构建为 0 错误、379 告警。CubeProgrammer 2.22.0 烧录校验成功。初次测试定位到 USART3 IRQ 入口已生成但未使能，在驱动开始接收时显式使能后重建和重测。COM4 逐档发送 0～4 均收到对应的 OK；每档用 SWD 读回 app_led 的完整 DMA 缓冲，验证全部 16 颗的 GRB 编码与 400 µs 复位尾部。测试了 CR/LF、非法字符 x 和 5、连续命令 2 后接 0、复位后默认 0。测试结束发送 0 并关闭串口。实际发光与视觉亮度待用户现场确认。 [命令及帧测试日志](led5-evidence/serial-levels.log) · [启动日志](led5-evidence/boot.log) · [烧录日志](led5-evidence/flash.log)
