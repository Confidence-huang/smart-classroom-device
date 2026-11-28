# STM32 / EC200U：4G 中断运行与恢复验证

## 范围与版本边界

2026-11-28 在已连接的 NUCLEO-F413ZH、EC200U-CN 和 K230 上完成本节四类故障注入。仅增加复测工具、原始脱敏记录和主机证据检查；没有修改 STM32 固件、公共消息、主题、控制优先级或云端 ACL，没有生成新 HEX 或烧录。

测试当时参考的设备仓快照是 `31ee6f21a9d4b7466e9d975535b4966319abf78a`。仓库日志将之前实板基线记为 `2f89a87`，但本次没有独立确定运行镜像的精确源码 SHA。不能将参考源码 SHA 直接当作实板身份。

PR 基于更新后的 `main`（`6b7f9c0`），保留其中 SGP30、LCD 与 I²C 恢复改动；下述硬件记录不自动成为这些较新改动的断网验收证据。板上运行版本变化后应重新验收，不能仅凭软件重放将结果迁移到新固件。

## 实板操作与结果

现场 STM32 日志口为 COM4，模组 USB AT 口为 COM25，USART6 为 STM32 的模组通信通道。串口号需要重新枚举。只读查询确认 EC200U 固件 `EC200UCNAAR03A17M08`、SIM READY、LTE 注册、PDP 与 MQTT 已连接。私有配置通过本机 CRC 检查，未导出到本 PR。

射频用例使用 USB AT `CFUN=0` 注入无线业务不可用，再由测试端 `CFUN=1` 撤销故障；后续注册检查、PDP、校时、TLS、MQTT 重建由 STM32 自动完成。这不表示固件会自行撤销人为关闭射频的设置，也不等价于运营商故障实测。

| 场景 | 注入操作 | 首次恢复 PUBACK | 之后观察 |
| --- | --- | --- | --- |
| 约 15 秒无线中断 | `AT+CFUN=0/1` | 恢复命令返回后 36.223 秒 | 150 秒内 23 条 |
| PDP 承载断开 | `AT+QIDEACT=1` | 断开命令返回后 4.685 秒 | 90 秒内 18 条 |
| MQTT 断开 | `AT+QMTCLOSE=0` | 断开命令返回后 6.357 秒 | 90 秒内 17 条 |
| 130 秒无线中断 | `AT+CFUN=0/1` | 恢复命令返回后 62.729 秒 | 150 秒内 18 条 |

四轮均持续收到视觉帧和环境采样，没有捕获启动横幅或健康任务停滞告警。130 秒关闭阶段收到 127 条视觉帧、26 次环境采样，正式 PUBACK 为零；整轮最大视觉/环境间隔为 1.059/5.970 秒。最长恢复等待包含现有最多约 60 秒加抖动的退避，不能承诺信号恢复立即上线。视觉帧有效性波动按原记录保留，不将无效帧当有效人数零。

采样持续证明所观察任务未被网络等待阻塞；本轮没有发送执行器命令，没有转速、电流或目视动作证据，不据此宣称风扇、灯环或 B1 按键现场验收通过。

北京时间 18:37:31 的收尾只读检查确认射频打开、LTE 注册、附着、PDP 激活、MQTT 连接均为真，见 [final-link-state.json](../../tests/embedded/evidence/2026-11-28-4g/final-link-state.json)。这是原测试结束时的状态，不是阅读本文时的实时状态。

## 证据与软件验证

证据目录：[tests/embedded/evidence/2026-11-28-4g](../../tests/embedded/evidence/2026-11-28-4g)。

- `rf-outage.json` 为首次脚本生成的原始数组；`pdp.json`、`mqtt.json`、`rf-130s.json` 包含原始记录和当时自动摘要，保持原文件字节不变。
- `host-control.txt`、`host-at-recovery.txt`、`host-publish-recovery.txt`、`host-codecs.txt` 为当时主机回归结果，包含租期回 AUTO、本地优先、AT 重试、发布、编解码与命令判重测试；主机通过不等于相应实板场景验收。
- `sha256.json` 记录上述原始证据文件的 SHA-256。
- `mqtt-observer.json` 记录一次本地只读观察尝试：TLS/MQTT 登录成功，订阅被设备账号权限拒绝，收到消息为零。未扩张权限。PUBACK 只证明 Broker 发布确认，不证明网页接收、LWT 展示或执行器业务 ACK。

主机重放检查不连接开发板，亦不需要 pyserial：

```sh
# 在 ecovision/ 目录执行；同一检查已加入现有主机 CI
python -B -m unittest discover -s tests/embedded -p 'test_4g_recovery_evidence.py' -v
```

它核对原始记录与摘要一致、短时用例统计、长中断期间采样与零发布、最终链路状态及观察失败边界；同时拒绝缺少采样/恢复回执、任务停止、启动横幅或健康停滞的证据。重放通过不算新增硬件通过。

## 后续复测工具

`tests/embedded/test_live_4g_recovery.py` 导入时不访问硬件。实际故障注入需要显式 `--run-hardware`，先有稳定 PUBACK，再注入；射频关闭后在 `finally` 尝试恢复。需要本机已安装 pyserial，两个串口分别对应实际 STM32 日志与 EC200U USB AT 端口。

```powershell
# 从 ecovision/ 执行。必须选择新的输出目录，避免覆盖归档证据。
python tests/embedded/test_live_4g_recovery.py --run-hardware --console COM4 --modem COM25 --case rf --outage-seconds 130 --observe-seconds 150 --output tests/embedded/evidence/new-rf-run
```

`--case pdp` / `mqtt` 分别单次断开 PDP/MQTT。工具只保存白名单日志及 USB 命令成功与否，不保存 USB 原始响应、APN、Broker 地址或凭据。PR 整理时补充了同端口/覆盖归档防护及健康停滞失败判定；这些工具改进仅经主机验证，四轮归档来自改进前的同一故障注入流程，本次整理 PR 没有重做故障注入。

## 尚未完成与下一步

断网不强制终止既有 REMOTE 租期，亦不续租，到期回 AUTO；LOCAL_OFF 仍须实体短按解除。控制代码的这些软件规则不代表本轮已实测断网中的 B1/租期、重连重复命令或恢复后业务 ACK。

仍需补充上述实板控制用例及云端接收，然后独立覆盖反复抖动、运营商长时间无服务/欠费、DNS/TLS/Broker 黑洞、模组掉电/无 AT 响应和 8 小时运行。现有软件重试不能替代恢复供电、资费或证书；没有自动切模组电源的验收结论。
