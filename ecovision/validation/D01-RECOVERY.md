# D01 net9验证候选：尚未解决持续上报超时

D01初始导入检查点25d2d25，源checkpoint5c74253，main基线0f8b9d6。应用net-20261122.9构建0错误456告警；定长发布恢复主机检查通过，应用HEX限定0x08000000..0x0800FF78（末地址不含），SWD读回一致。HEX SHA256为6798bab343d53ff58081af9db50a2bb7ecb01f1dee1be3cd7d85746f71e1939f。应用/配置分区不变，凭据未入库。

旧net8一小时收到670条正式消息（计划720），原始收包数比例93.1%，不达95%。net9将缺失提示符时的原定长度载荷只补齐一次，必须收到匹配PUBACK才算成功；这是有主机测试的恢复尝试，硬件仍出现rx=0、无提示符/无PUBACK和AT无响应，不能称为已修复。USB AT在故障时仍可读，QSCLK0、115200、IFC0/0、CEREG0/1、CPSMS0。物理链路及模组内部状态仍待区分。

该窗口出现一次MCU复位，原因未确认；其后有受控USB MQTT关闭和一次CFUN复位，已将窗口中止记FAIL，不是完整一小时测试。首次HOTPLUG擦除失败后改UR/halt烧录并校验成功，失败日志保留。

总控本地证据路径为docs/verification/2026-11-22（在总控工作区，不在本仓库根）：d01-recovery-identity.json、d01-recovery-flash-ur.log、d01-recovery-window-failed.json、d01-recovery-uplink.json、ec200u-net9-fault-readonly.json及d01-checks/。完整结论见总控RESULTS.md。

本分支为验证候选；没有PR或推送，也未合入main。解决主UART稳定性后需要最终固定固件的一小时、复位/掉电与云端关联记录。原src/的MicroPython未改，现场当前运行D02 local-auto-20261122.2，不能把net9称为当前实板版本。

## .11恢复流程修正（后续候选）

当前候选版本/哈希见CANDIDATE.json。发现正常已连接链路丢回复后仍扫描全波特率并留在9600退避，现由net_recovery.c集中控制：已确认链路仅按原速率重试，不作PPP逃逸；兼容扫描仅用于首次连接，失败后恢复115200。状态恢复主机测试通过，Keil0错误458告警（新增结构对齐告警未隐藏）。这修复了恢复延迟放大问题，不证明模组底层偶发无响应消失；新实板一小时与复位恢复仍待完成。
