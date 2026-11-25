# 公共契约变更记录

## 1.1.0 — 五档远控候选

增加独立的 MQTT 遥测、命令和 ACK schema。设备显式启用并上报 1.1.0 后可使用五档。保留冻结的 1.0.0 schema 与二值回退。主题、UART 与状态报文沿用原接口。

## 1.0.0 — 2026-11-12

总控首次冻结：保留ecovision/{device_id}/telemetry、cmd、ack、status主题及AUTO/REMOTE/LOCAL_OFF优先级；补齐字段、单位、ACK原因、时效、幂等及版本规则。

相对于旧20天文档的候选EV1五字段，EV1新增link_id、vision_boot_id；新增EVQ/EVH握手以明确重启会话、防止缓冲旧帧刷新感知。不兼容旧候选串口格式，当前目录没有已实现的旧协议代码，所有模块从本版开始。新增control_epoch以拒绝按键前的在途远控。版号固定1.0.0，后续修改由总控统一发布。

### 1.2.0 — Simulated AC commands

- Fresh v1.2 telemetry advertises simulated AC support. It has the same payload fields as v1.1 telemetry.
- v1.2 command uses the same MQTT command topic and session context, with `ac_temp_c`: 0 means OFF and integers 16–30 mean ON at the selected temperature. Fan/lamp commands continue using v1.0 or v1.1.
- `AC_ACCEPTED` means the learning module acknowledged the request. `AC_UNKNOWN` means the UART exchange could not confirm the result. F103 acts as a one-way display and remains a separate verification point.
- Deploy a console that understands v1.2 before enabling it on the device. The F413 advertises it only with the v1.1 private configuration, remote commands enabled, and a successful boot-time ZJ-IR-V2 probe.
