# 2026-11-24 离线优化候选与上板清单

基于模块PR #5候选 d120970，新建本地分支 `feat/stm32-dma-watchdog-20261124`。未上传、未合并main、未修改原PR。原模块候选和工作区保留，可回退到原镜像。全部接线沿用 MODULE-WIRING.md，不新增外部引脚。

## 已完成

- 修正CRC移位有符号提升及net_codec两处易误读缩进；开启AC6业务源码 `-Wall -Wextra`，最终全量构建日志0条warning/error。没有全局屏蔽HAL/FreeRTOS警告；未重跑uVision IDE的历史375/481告警计数，不能宣称那些告警已全部消除。
- USART2 RX=DMA1 Stream5 Channel4；USART6 RX=DMA2 Stream1 Channel5，均512字节、循环模式、字节宽度、内存递增、高优先级，HT/TC与UART IDLE同时保留。IRQ优先级6，无每字节RX中断，灯环DMA1 Stream7不变。依据[ST RM0430表30/31](https://www.st.com/resource/en/reference_manual/rm0430-stm32f413423-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)及工程自带HAL实现。
- ISR仅把新增区间复制到已有环形缓存，任务继续解析EVQ/EVH/EV1和AT；IDLE不是帧边界。按当前NDTR读位置，避免延迟HT通知重复搬运。两个DMA边界同时待处理时保守丢弃并置错；上层任务清缓存、恢复接收，视觉重新握手。保留CRC、重复/会话/超时保护。只有接收改DMA，发送暂保留原有有限超时HAL调用；LCD SPI/DMA不在本轮范围。
- DMA中断必须在半缓冲填满前及时服务：115200/8N1时约22ms；这不是无限缓存，异常持续屏蔽中断仍须实测丢弃与恢复。回调中的时间戳为批次到达时间，存在最多半缓冲/IDLE调度延迟，不能当作逐字节物理捕获时间。
- 单一IWDG监督者在heartbeat任务执行，启动宽限8s，检查control/vision/net/env/IR任务进展，门限分别1/3/5/5/2s；全部任务正常才喂狗。门限到达锁存失败，恢复beat也不会重新喂狗，等待硬件复位。缺席任务不能无限绕过启动保护。
- /256、reload1999，32kHz LSI名义超时16s，实际受LSI误差影响。仅记录复位标志并清除，不伪造精确复位时间。异常先检测到超时，再停止喂狗，最终复位还要等IWDG剩余时间。
- 联网等待拆为至多100ms睡眠片段并报告任务存活，网络断开、配置未注入、传感器失败及视觉掉线不会仅因数据无效导致重启。阻塞/死循环停止任务进展则不再喂狗。启动宽限前及监督任务首次启动前的故障还不受本方案覆盖，未声称全启动链保护。
- 队友 `feat/stm32-alert-watchdog-tts` 的告警/语音与AppHealth未合入；未来合并时必须只保留一套IWDG启动和喂狗所有者，不能同时启用两套。

## 软件验证

13组C主机程序通过（既有9组＋cursor、health_policy、真实UART DMA适配器、真实watchdog适配器），覆盖重复/延迟事件、部分缓冲/回卷、双边界丢弃、端口隔离、重启失败、启动宽限、复位原因、长等待拆分、停止喂狗及故障锁存。17项契约、32项K230测试、实际LED任务失败退出回归通过。主机模拟不证明DMA电气收发、IWDG真实超时或任务栈余量。

正式源码AC6构建68对象：Code=51622、RO=6174、RW=44、ZI=102516，0条诊断。HEX地址结束0x0800E1F8，所有数据记录校验和通过、未覆盖0x08160000起配置区。

固件标识 `dma-watchdog-20261124.1`。

可烧录候选（仓库根相对路径）：
`ecovision/firmware/stm32/ir-merge-evidence/2026-11-24/optimization/EcoVision-dma-watchdog-20261124.1.hex`

SHA256：`5b901b05241ed62cfea776108133d9f113abf130060dfa47a36b386cde731aab`。

以上本地输出被忽略，未上传。不要选择旧IR候选、IDE目录历史HEX或下述再生成验证镜像烧录。没有把私有配置或整片Flash打包。

CubeMX6.15在临时副本成功重新生成，DMA映射和USER钩子保留；使用副本main/IRQ/MSP/Inc全量链接68对象通过。首次因缺失忽略的uvoptx而生成失败，补齐临时副本后重试成功，原工程未覆盖。生成副本编译大小及哈希见 `ir-merge-evidence/2026-11-24/cubemx-build/regenerated/image-check.json`，不与正式候选混用。CubeMX可能创建MSP DMA句柄，运行时适配器会重新绑定自己唯一的活动句柄；生成的IRQ保留USER回调，不能手动再追加同名IRQ。

## 复现

在仓库根执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ecovision/firmware/stm32/tests/test_optimization.ps1
python -B ecovision/firmware/stm32/tests/test_led_task_exit.py
python ecovision/firmware/stm32/tests/build_ir_pulse_ac6.py --output ecovision/firmware/stm32/ir-merge-evidence/2026-11-24/optimization
```

Linux CI入口仍为 `cd ecovision && bash firmware/stm32/tests/run_host_ci.sh`，已增加四组测试；本轮分支未推送，因此没有本轮GitHub CI结果。Windows脚本需VS2022 Community，AC6默认D:/k51/ARM/ARMCLANG/bin，可用--toolchain指定。Python协议回归同既有MODULES-PR-VALIDATION.md。

## 明天上板顺序（2026-11-25）

1. 保留现用固件和配置。先断开电机负载，核对原有接线及电源；使用上述指定HEX，经应用区下载+校验，不做整片擦除，不覆盖末128KiB配置区。本轮未连接开发板，未烧录。
2. 打开ST-LINK当前COM口，115200/8N1，记录完整启动日志：版本必须dma-watchdog-20261124.1；看[WDT] reset_flags、8s后started及health=1。先运行10分钟，正常不应周期性重启；一旦stop feeding或反复重启，保存日志并回退，先核查任务进展而非延长超时掩盖问题。
3. 先单测K230：运行figurine_uart.py，确认EVH/EV1、新人数及CRC处理；分别断开/恢复K230串口。掉线显示未知，不能显示无人，恢复后新会话重新握手。缺少K230时任务应仍活跃，不能因此看门狗重启。
4. 再单测EC200U：确认实际AT路径及波特率探测、真实入网/上报与订阅端对应；拔天线/断网、缺配置、长重连等待期间，B1/本地安全关闭仍可用且不因网络等待导致复位。不要只看PUBACK日志代替订阅端实收。
5. 两路同时运行：测试跨256/512字节边界的分片、粘包与持续数据，记录RX fault/重握手、UART错误和真实发布；禁止往真实模组AT口盲发随机数据。受控UART测试仪/临时台架可用于错误和过载注入，确认丢弃后恢复、不接受错帧。
6. 接回灯环/风扇低压负载，验证上电默认关闭、0/F0安全关闭与B1/AUTO行为，观察电机启动是否拉低VM或引发整机复位。当前main仍拒绝旧1～4/F1～F4台架命令。
7. 最后由联机调试协助暂停关键任务，验证进展门限触发、停止喂狗、硬件复位、下次日志iwdg=1及输出默认关闭。调试暂停CPU时IWDG可能继续运行，禁止把手动复位/断电当作看门狗通过。默认固件未加入任何会主动卡死的串口命令。
8. 至少保留版本、HEX哈希、复位原因、双UART原始日志、云端订阅记录和实物动作视频。确认后再决定上传新分支/开优化PR，不直接替换或合并原模块PR。

待实测：DMA实际波形与连续负载、错误恢复、ISR最大延迟、CPU占用、任务栈/堆余量、IWDG实测超时与输出复位状态。外置温湿度型号、电平等既有未确认项继续保留。
