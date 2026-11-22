# K230 模型小人数 → STM32 UART

## 2026-11-22 D02 当前候选

当前 STM32 候选为 `local-auto-20261122.2`，新增最小 AUTO，继承 EC200U 上报；原始文件下方的 11 月 20 日结果是历史证据。当前串口接线已按内侧排针实测恢复：K230 Pin11/GPIO5 TX → STM32 PD6/CN9-4；K230 Pin13/GPIO6 RX ← STM32 PD5/CN9-6；GND 共地。不要把外侧排针历史位置当成本轮接线要求。

运行入口仍为 `/sdcard/ecovision_uart/figurine_uart.py`，置信度0.35、NMS0.3。模型二进制SHA256与 selected/ 一致；六个脚本只有换行差异，不能写成原始字节哈希一致。未覆盖板端 main.py/boot.py，IDE后端断开会停止本次脚本，断电/断开后的自启尚未通过。

本轮用户同意计数小幅误差不作为PR门槛，但空场景误报仍影响30秒无人关闭。当前模型小人台架不代表真人精度验收；灯和风扇尚未接入，实际动作未通过。最新记录见 `../../validation/D02.md`，公共1.0.0字段和计时规则未变。历史提及的整片Flash备份不随本功能分支公开。

## 11月20日历史记录（不作为当前固件身份）

2026-11-20：本次只接人数通信，不启动训练，不增加灯/风扇自动控制。

## 当前入口

在 CanMV 中运行板上 `/sdcard/ecovision_uart/figurine_uart.py`。

电脑源文件为本目录 `figurine_uart.py`，复用 `/sdcard/kmodel/figurine_multi.kmodel`，输入 320×320、置信度 0.35、NMS 0.3。当前原始计数仍不稳定；通信收到这些数字并不证明识别准确。

配套模块需一起上传到 `/sdcard/ecovision_uart/`：

- `protocol.py`：冻结 EVQ/EVH/EV1 与 CRC。
- `figurine_uart_link.py`：轮询接收、每秒发最新样本、短写处理、记录两次发送之间的故障。
- `runtime_identity.py`：随机启动标识及历史账本。
- `uart_board.py`：GPIO5/6 UART2、扩展单调毫秒。
- `uart_smoke.py`：明确标记为模拟数据的真实串口测试入口。

识别程序在运行时显示计数并每秒发送 EV1。STM32 先发送 EVQ、K230 返回 EVH，之后才发送数据。空检测结果为有效 0；推理异常或超过协议 20 人上限为无效占位 0，不裁剪人数。

## 接线

| K230 40Pin | STM32 NUCLEO-F413ZH |
|---|---|
| 物理 Pin11 / GPIO5 / UART2_TX | PD6 / USART2_RX，内侧CN9-4 |
| 物理 Pin13 / GPIO6 / UART2_RX | PD5 / USART2_TX，内侧CN9-6 |
| 物理 Pin9 / GND | GND |

两板独立供电，3.3V TTL。此次起初误接 PD7，用户改为 PD5 后握手成功。GPIO5→PD6 的高低电平曾通过 GPIOD IDR 只读采样验证。

## 已验证

- 32 项 Python 主机测试；C 接收器主机断言测试；Keil 最终固件编译 0 错误。
- 真实串口测试 0、3、12 人与无效状态，明确区分有效 0 和无效 0。
- 真实摄像头结果发到 STM32；归档窗口中连续 25 帧（seq 5～29）与 K230 TX 的 uptime/count/valid/link/boot 全字段一致，CRC 正确。
- 停止 K230 后约 5 秒 STM32 使旧人数失效、生成新 link；恢复程序后重新握手。
- STM32 复位后换 link，K230 不重置 seq；K230 程序重启后换 boot，重新握手。
- 最终入口修复了 media 星号导入造成的 sys 名称遮蔽，并保证 IDE interrupt 向外传播；实测可停止，停止后 script_running=false，再启动后收发恢复。

## 固件兼容及限制

CanMV v1.8-66 没有 `os.sync()`。保留旧 `boot_identity.py` 的失败关闭行为，新的入口使用 OS 随机 64 位 ID，并在 `.boot_id_ledger` 中拒绝重复标识或损坏历史。不能删除或复制该账本到另一活动设备。此固件无法显式同步目录/文件系统，因此不宣称账本已经通过突然断电持久性验证；每次启动还要求新的 OS 熵。实际多次软重启标识均不同。后续断电测试需单独记录。

没有覆盖 K230 `main.py` 或 `boot.py`，本次未配置开机自启。断电后需手动运行上述入口。未完成 30 分钟连续验收、8 小时长测、物理拔插线故障或识别精度验收；停止程序模拟断流不等同物理拔线测试。

## STM32 与日志

STM32 当前固件：`../stm32/environment-evidence/EcoVision-uart-env-lcd-20261120.3.hex`，已烧录、校验，参考组员布局恢复真实 AHT20 温湿度、PC5 光敏 ADC，并显示 K230 人数。用户已确认四项正常；详见 `../stm32/ENVIRONMENT-DISPLAY-RESULTS.md`。旧固件仍保留。USART2 IRQ + 1024 槽接收环，独立任务，不联动负载。USART3 PC 调试、UART4 红外保留。RTOS 堆为 12288 字节；当前启动采样空闲堆 4064 字节、视觉任务剩余栈 2520 字节，仅代表启动时采样。

STM32 PC 端 COM4，115200/8N1。日志示例：

```text
[VISION] RX seq=9 count=15 valid=1 uptime=10524 link=... boot=...
```

valid=1 只表示采集/推理与范围检查通过，不代表人数已经人工核对。

原 STM32 完整 Flash（1572864 字节）保存在 `../stm32/uart-evidence/pre-uart-flash.bin`；原工程修改前文件在 `source-before/`，保留备份，不把新 C 固件当作原先 MicroPython 固件。

原始证据位于 `../stm32/uart-evidence/`：`smoke-wiring-corrected.log`、`timeout-and-real-vision.log`、`final-restart-and-runtime.log`、`k230-restart-final.log`、`k230-final-tx.log`、`wire-comparison.json`、`final-current-running.log`、`flash-final.log`。

复核命令（项目根目录）：

```powershell
& 'D:\codex richang\k230-learning\.venv-train\Scripts\python.exe' -B -m unittest discover -s tests/vision -v
& firmware\stm32\tests\run_vision.ps1
& 'D:\codex richang\k230-learning\.venv-train\Scripts\python.exe' firmware\stm32\tests\verify_uart_evidence.py
```
