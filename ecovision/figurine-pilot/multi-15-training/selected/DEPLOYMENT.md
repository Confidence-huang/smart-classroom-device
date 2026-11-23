# 多人候选模型部署记录

日期：2026-11-19。

- 开发板：LCKFB-LSPI-K230-1G-CanMV，COM15。
- 固件：v1.8-66-g2d060bef54b161c3ac65a1c4c1e2789d4ea8a544，nncase 2.11.0。
- 源权重：figurine_multi_candidate.pt（multi320/weights/epoch10.pt）。
- 输入：RGB、320×320、float32 /255。校准使用 91 张训练图片，等比缩放后右侧/底部填充值 128，与现有板端 letterbox 对齐。
- 模型：/sdcard/kmodel/figurine_multi.kmodel，3480680 字节。
- SHA256：16e94f6ec170c6eb162a606a1e9c55dca5291d6f5454a6cb2ffb95b7db4f1034。本机哈希与串口读取板端哈希一致。
- 程序：/sdcard/figurine_count_multi.py，已写入并启动。
- 摄像头：GC2093 CSI2，sensor_id=2，输入 640×480。
- 置信度阈值 0.35；NMS 0.5；最多保留 30 个检测框，无人数裁剪。
- 现场结果：保存画面可见 7 个小人，各有检测框；连续 26 次抽样输出为 7。冷启动首段 10.4 FPS，此后约 14.3～15.3 FPS。
- CanMV script_running 返回 true；预览帧正常。证据截图：k230-multi-live.jpg。
- 原 V4 文件保留。本次未设置开机自动执行。

限制：这证明新量化模型能在当前七人场景实时运行，尚未完成 0～15 人逐档硬件验证，也未证明其独立测试准确率。32/32 是选模型使用过的开发验证结果。下一步见 ../NEXT-TRAINING.md。
