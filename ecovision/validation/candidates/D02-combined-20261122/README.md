# D02 联合候选 HEX（仅交付，未烧录）

本候选由D02原分支同步已合入D01的main后构建，供后续同一HEX实板短测使用。

- 主分支基线：`ee86e0c9151b1cdc791ab5a58dcda86ddc314ac2`，D01 PR #1已合并。
- 合并前D02：`ade40642018399f5aaa7b8190cd8295a57c4415f`。
- **构建源码提交：`fd67c806b660dc6e215ad5e561ebc5ada71bad98`**。后续证据提交只更新候选/日志/文档和此HEX，不改变被构建的固件源码。
- 已同时包含：D01 `StartLedTask()`的CMSIS安全退出；D02内部无效视觉帧计数及AUTO连续确认中断。
- [联合固件](EcoVision_F413ZH.hex)，SHA256：`1682b08549e2e47a8a2d0fb24f074293bf05baf398b7e6999f5fc769bc7ffd47`。校验清单见SHA256SUMS.txt，全局候选元数据见../../CANDIDATE.json。

目录内.gitattributes仅对这份HEX设置`-text -diff`，保留构建产物的原始字节并按产物显示，避免Git自动转换换行后改变下载文件的SHA256。

## 构建与软件验证

| 检查 | 本次结果 | 证据 |
|---|---|---|
| 控制策略及适配器 | PASS，包含无效帧覆盖边界 | control-regression.txt |
| 视觉C协议 | PASS，包含合法无效帧计数及重置 | vision-c-regression.txt |
| 实际LED任务入口失败返回 | PASS，调用CMSIS退出 | led-exit-regression.txt |
| 公共契约 | 17项PASS | contracts-17.txt |
| K230主机测试 | 32项PASS | k230-host-32.txt |
| Keil完整重新构建 | **0错误、481告警**；Code61938/RO5862/RW44/ZI94196 | keil-build.txt |

构建命令（设备仓根）：`ecovision/firmware/stm32/tests/build_fan.ps1 -EvidenceName D02-COMBINED-20261122`。脚本名称沿用既有入口，实际构建完整EcoVision_F413ZH目标。

解析HEX后应用范围为0x08000000..0x08010908（末端不含），跨度67848字节，不包含0x08160000私有配置。hex-proof.json仅为文件解析证明；其中readback_size是计算出的应用跨度，**不是已执行硬件读回**。未写板端、未读私有配置、未烧录或操作负载。

启动banner保留`local-auto-20261122.4`，不能只凭banner区分旧镜像，必须对照源码提交和上述HEX SHA256。旧候选哈希保留在CANDIDATE.json的previous_candidate内，不用于本次短测。

## 后续实板短测（本步未执行）

后续只用本目录这一份HEX核对1–2分钟新鲜真实4G上报、K230连续3帧确认、AUTO灯扇条件及B1长按LOCAL_OFF实际关闭/另次短按AUTO。温度未达到28℃持续5秒时，风扇自动起转记录未覆盖；可选断流约5秒UNKNOWN、连续关键故障60秒关闭和恢复重确认。没测到的直接记未测，不把电脑回归、旧实板证据或本次构建当作新硬件通过。

上报间歇中断根因仍未宣称解决。D02由队友在短测通过后决定合并；本次只交付联合HEX及验证资料，不重复旧一小时/八小时实验。
