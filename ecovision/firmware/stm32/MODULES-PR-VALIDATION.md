# 模块 PR 软件验证记录

2026-11-23，基线 `origin/main 8cba6ef0a89a1f965f4a766b8a0df14a92db9790`；独立分支 `feat/stm32-modules-ir-wiring-20261123`。固件启动标识 `ir-pulse-main-20261123.1`。对应PR的精确提交以Git与PR head为准，不在同一提交中自引用SHA。

## 审查与合入范围

灯环、TB6612、干簧管和BH1750驱动已在main，本PR保留其实现，新增统一接线交接。新移植的是队友红外包v0.3的自定义24位演示协议与单帧学习/重放；原始F407/F103工程保留，不直接覆盖F413初始化。不引入重复传感器、SGP30或另一套自动控制。

- 原TIM3载波与风扇冲突、TIM2接收与灯环冲突：改TIM1_CH2 PE11载波、TIM4包络、TIM5_CH3 PA2捕获。时基从APB计算，TIMPRE不支持时拒绝初始化。
- ISR仅推进包络或缓存边沿；环缓存溢出/过捕获丢帧，主任务解码。队列8条、学习10s、发送1.7s超时，等待不阻塞本地控制。
- 解码要求51段、边界合法的mark/space、结束mark、异或及参数校验，避免原包接受任意长space。原始帧各100～15000us、总≤1.5s、奇数相位及容量限制。
- 接入main的 `ControlOutput/CONTROL_LOCAL_OFF`，不使用共享目录旧版控制API；LOCAL_OFF停止发射、清除请求。日志分别记录排队、序列推进完毕，不返回目标执行成功或云端ACK。
- 保留D01的LED失败任务退出、D02无效帧计数和当前AUTO输出所有权。旧台架1～4/F1～F4仍拒绝。
- `.ioc`保存新资源，手写任务声明/包装函数移入USER区，避免视觉任务再生成重复；HAL配置USER Header保留LCD的SPI模块。未在原工程上运行CubeMX覆盖；本PR未重新执行CubeMX副本再生成，仍需维护者再生成差异验收。
- 旧YS-IRTM任务不启动，其UART回调分发保留；公共1.0.0字段、MQTT主题、优先级及私有配置区不改变。

## 本轮结果（软件，不等于实物）

| 检查 | 结果 |
| --- | --- |
| 9组C主机程序 | 通过：vision_protocol、net_codec、net_recovery、net_publish、control_policy、control_adapter、ir_decode、ir_encode、ir_driver |
| 真实StartLedTask故障返回回归 | 通过，仍通过CMSIS退出 |
| 公共契约Python | 17项通过 |
| K230主机Python | 32项通过 |
| 完整AC6编译链接 | 64对象，工具退出0；Code=49018，RO=6122，RW=44，ZI=101244；本轮不是uVision IDE重建 |
| Intel HEX | 记录校验和及应用地址范围检查通过，结束地址0x0800D798；保留0x08160000起的末128KiB配置区 |
| 固件烧录、供电、动作、红外载波与接收 | 本轮均未执行 |

生成HEX的SHA256：`0b7dff93372a62eedabb18d3995f9bad0cd90dd022c4b157177d8646d80b04fc`。镜像与完整机器日志在本地忽略的 `firmware/stm32/ir-merge-evidence/2026-11-23/`，不将旧实板日志或整片Flash混入PR。该哈希对应本轮源码构建结果，不代表已经烧录。

## 复现

从仓库 `ecovision/` 运行Linux CI：

```sh
bash firmware/stm32/tests/run_host_ci.sh
```

Windows红外回归及完整AC6构建（从仓库根）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ecovision/firmware/stm32/tests/test_ir_pulse.ps1
python -B ecovision/firmware/stm32/tests/test_led_task_exit.py
python ecovision/firmware/stm32/tests/build_ir_pulse_ac6.py --toolchain D:/k51/ARM/ARMCLANG/bin
```

MSVC脚本默认VS2022 Community本机路径，可按本机vcvars调整；完整构建可传 `--toolchain` / `--output`，使用受版本管理的 `MDK-ARM/application.sct`，不依赖被忽略的IDE产物。9组C回归本机使用MSVC C11 `/W4 /WX /D_CRT_SECURE_NO_WARNINGS`；首次未加兼容宏因既有标准C strcpy/sscanf的C4996提示失败，加兼容宏后全部通过。CI使用原有GCC C11/Wall/Wextra，不关测试断言。

Python协议回归从 `ecovision/`：

```powershell
python -B -m unittest discover -s tests/integration -p 'test_*.py' -v
python -B -m unittest discover -s tests/vision -p 'test_*.py' -v
```

## 待队友验收

按 [MODULE-WIRING.md](MODULE-WIRING.md) 完成实物接线和逐项观测。独立红外模块实物供电/有效电平尚未确认，SHT型号未确定；只有接线与电平确认后才上板。本PR待评审和硬件验收，不自动合并main。真实空调须另核品牌型号、遥控器与波形，自定义24位演示不能冒充真实空调控制。
