# D02 无效帧快照覆盖修复

审查来源：[PR #2 / 7080789667](https://github.com/Confidence-huang/smart-classroom-device/pull/2#issuecomment-7080789667)，基线1225981。原五文件补丁在原分支应用前`git apply --check`通过；没有修改公共1.0.0协议、主题或优先级。

## 问题及修改

AUTO两次读取快照之间，已接收的合法valid=0帧可能被下一帧有效数据盖掉，导致连续有效帧确认未被中断。接收器增加内部`invalid_frames`计数，仅合法接受的无效样本递增，建立新link时归零；AUTO观察计数变化后先中断确认，再累计后续有效帧。不是新增串口或MQTT公共字段。

## 本机验证

1. 先应用审查者的两个测试文件及计数结构声明，不改生产逻辑。`test_control.ps1 -EvidenceDir .../review-d02-red`：策略检查通过，但适配器在`UNKNOWN && !led`断言失败，完整命令退出1。见control-red.txt。
2. 应用app_control.c和vision_protocol.c的生产补丁后，重跑同一控制检查：策略及适配器通过，完整命令退出0。见control-green.txt。恢复有效帧仍需重新累计，不会借覆盖跳过中断。
3. 现有`run_vision.ps1`：CRC、握手、合法无效帧计数保留/新link归零及既有序号/超时边界通过。
4. `python -B -m unittest discover -s tests/integration -p 'test_*.py' -v`：17项通过。
5. `python -B -m unittest discover -s tests/vision -v`：32项通过。

以上为本机软件结果，不是新固件实板通过。修改后尚无新Keil HEX；CANDIDATE.json中旧HEX已移至previous_candidate，防止把旧构建当成新源码。

## 后续顺序

等待维护者复审并合入D01修复dbbb873；随后核对实际main，改PR #2 base为main并同步最新main到原D02分支，检查两处修复都在。再Keil Build，固定提交SHA和HEX SHA256，用同一HEX短测：1–2分钟新启动遥测、K230三帧确认、实际温度28℃持续5秒风扇条件、B1长按LOCAL_OFF实际关闭及短按回AUTO。可选断流约5秒UNKNOWN、连续故障60秒两路0、恢复重确认。

未达到温度或未观察的动作直接记录未覆盖。旧实板/长测记录不改写，不新增一小时或八小时要求；作者不代维护者合并。
