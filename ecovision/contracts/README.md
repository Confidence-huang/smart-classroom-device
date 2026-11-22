# 公共契约 1.0.0

由总控维护。先读docs/INTERFACES.md，再使用*.schema.json、policy.json和examples/。JSON Schema只描述形状，跨字段、幂等与状态迁移以接口文档为准；examples/scenarios.json为有状态验收输入/预期，示例预期不是实际设备运行结果。

schema无引用依赖，采用Draft 2020-12。示例时间固定为2026-11-14 12:00:00 UTC（北京时间20:00），仅测试夹具，不能原样发送到现在的实物。真实命令必须重新填写当前时间、设备boot_id/control_epoch及新cmd_id。

uart.json中的wire包含真实CRLF转义；bad_crc特意错误。invalid_command.json特意不符合schema；telemetry_invalid.json则是结构正确的感知无效遥测。telemetry_offline.json是断网本地快照，不是成功MQTT发送的证据。
