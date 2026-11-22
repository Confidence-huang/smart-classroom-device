# D01 LED任务错误退出修复

审查来源：[PR #1 / 7080788516](https://github.com/Confidence-huang/smart-classroom-device/pull/1#issuecomment-7080788516)，基线9955939。仅应用原main.c两行补丁，并新增故障返回回归；未更改冻结公共协议或根src/旧固件。

`AppLed_Run()`在初始化或首次关闭帧失败时返回。移植层`prvTaskExitError()`不允许任务直接返回；`StartLedTask()`改为调用`osThreadExit()`，本配置下通过`vTaskDelete(NULL)`结束自身。该缺陷已从源码证实，但当前视觉/环境任务仍运行的间歇上报故障不能直接归因于它。

## 本机验证

- `python ecovision/firmware/stm32/tests/test_led_task_exit.py`：从真实main.c提取任务入口，用返回的LED桩与不返回的CMSIS退出桩编译运行。补丁前输出`FAIL: LED task returned instead of exiting through CMSIS`、退出1；补丁后完整命令退出0并输出PASS。不等于FreeRTOS硬件故障注入通过。
- 测试启动器初次遇Windows命令引号错误；中间一次行为断言已通过但临时目录被子进程工作目录占用，完整命令仍失败。修正批处理启动及工作目录后再次运行，最终退出0；没有把中间失败计为通过。
- `ecovision/firmware/stm32/tests/build_fan.ps1 -EvidenceName D01-REVIEW-20261122`：Keil完整重建0错误、459告警，Code60310/RO5682/RW44/ZI94076。告警未清零；新增一条StartLedTask可标注noreturn的提示。
- HEX SHA256：`97cd57644809805e22d956fadc86e8c3db488c8f1664f4613343a41c9fd60840`。应用范围0x08000000..0x080101F8（末端不含），不包含0x08160000私有配置区。见hex-proof.json及keil-build.txt。
- 启动banner保留net-20261122.11；新旧镜像以提交SHA和HEX哈希区分。

## 未完成及下一步

本修复镜像未烧录，不能沿用旧实板记录声称新镜像通过。先由审查者复核并合入D01；随后D02同步main，把本修复和无效帧计数修复合到同一份候选，重新Build并用同一HEX做用户指定短测。作者不代审查者批准或合并。
