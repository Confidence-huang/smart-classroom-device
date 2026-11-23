# D02：K230 UART与最小本地AUTO

从远端 main `0f8b9d6` 准备独立功能范围，源码取自 checkpoint `5c74253`。只引入必要 STM32 工程、SDK/RTOS、契约、主机检查和网络历史证据；不引入整条 checkpoint 的算法、训练、云端和沙盒。

当前新增本地控制入口和限制见 [D02验证记录](validation/D02.md)；STM32候选local-auto-20261122.2，未实现远控/业务ACK。下文D01构建/配置分区流程仍适用。

## 构建与软件检查

在本 ecovision/ 目录执行，工具链需要 Keil MDK/Arm Compiler 6.16（本机 D:/k51）及 Visual Studio 2022 C 编译环境；以下是本机已验证入口，其他电脑需调整脚本工具路径：

```powershell
& firmware/stm32/tests/build_fan.ps1 -EvidenceName D01-20261122
& firmware/stm32/tests/test_net.ps1
& firmware/stm32/tests/run_vision.ps1
& firmware/stm32/tests/run_environment.ps1
& firmware/stm32/tests/run_lcd.ps1
& firmware/stm32/tests/test_fan.ps1
& firmware/stm32/tests/test_bh1750.ps1
& firmware/stm32/tests/test_reed.ps1
python -B -m unittest discover -s tests/integration -p 'test_*.py' -v
```

build_fan.ps1 沿用历史脚本名称，实际重建完整 C/HAL 工程，不是只编译风扇。它不烧录。CubeMX .ioc 与源码一起保存，当前验证是跟踪源码的 Keil 复建，不代表重新生成代码已经验证；再次生成前保护当前源码并审查 diff。

协议权威：[统一接口](docs/INTERFACES.md)、[公共结构](contracts/README.md)。本D02副本已引入K230运行包与模型，视觉32项、公共契约17项通过；软件通过不替代本地负载、空场景与稳定性验收。

## 配置、运行与证据

- [网络说明](firmware/stm32/NET-UPLINK.md)、[引脚资源](firmware/stm32/PHASE1-RESOURCES.md)、[D01 软件结果](validation/D01.md)。
- 应用链接区域为 0x08000000 起 0x160000 字节；私有配置区自 0x08160000 起，应用 HEX 不包含该区。禁止全片擦除后忘记恢复私有配置。
- 私有设置/CA/整片 Flash 不随源码提供，未复制既有秘密到此功能分支。实板已配置时不能为验证随意覆盖；新设备的配置需沿用受控本地流程，不能在源码填凭据。
- 归档 net-evidence/2026-11-21 是历史硬件证据，本轮回放不算新增实测。tests/observe_net.cjs 仍是历史短时观察工具，依赖另行提供的 MQTT Node 模块及受控本地配置；当前 D01 软件构建不依赖它，不能拿它直接宣称已完成 1 小时持续验证。
- 当前人数可以 UNKNOWN；本地 AUTO、远控业务 ACK、整机故障恢复与 1 小时/8 小时的完成状态分别核对。

用户要求 D01/D02 完整验证后再开 PR，故当前 PR 门槛保持 BLOCKED，不创建 Draft PR。两套固件与旧证据保留，后续仅按选定范围提交审查。

## 模块接线交接

队友请先读 [固定接线与验收清单](firmware/stm32/MODULE-WIRING.md)。独立红外收发为新候选，外置温湿度仅预留；软件验证与硬件观测分开记录。
