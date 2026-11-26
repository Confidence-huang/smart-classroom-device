# 2026-11-24 K230 排障脚本归档说明（仅参考，不可部署）

> [!WARNING]
> 本目录下的 `k230-live-fallback.py` 与 `k230-sd-main.py` **仅作 2026-11-24 现场排障历史参考，未用于当前生产部署，也不可直接覆盖生产环境**。

---

## 1. 当前 `main` 上 K230 离线自启的正式实现（PR #17）

K230 脱离电脑 CanMV IDE 后的冷启动与离线连续上报问题已在 `main` 由 PR #17 修复，生产部署以 `ecovision/firmware/k230/` 及板载 `/sdcard/ecovision_uart/` 为准，具体机制如下：

1. **开机入口**：上电未连接 IDE 时，CanMV 自动执行 `/sdcard/main.py`，拉起 `/sdcard/ecovision_uart/figurine_uart.py`。
2. **启动标识分配与账本去重**（见 `ecovision/firmware/k230/runtime_identity.py` 的 `reserve_random_id`）：
   - 优先通过 `os.urandom(8)` 生成 16 位十六进制 `boot_id`，最多尝试 16 次，并与 `/sdcard` 账本中的历史 `used_ids` 比对去重。
   - **冷启动随机熵重复兜底**：若 16 次随机均命中历史账本，**不再关闭 UART**，而是读取账本末条记录 `history[-1]`，执行 `(number + 1) & 0xffffffffffffffff` 顺延递增直至找到未使用 ID，追加写入账本后继续初始化串口。
3. **离线串口通信**：初始化完成后，通过 USART2 向 STM32 F413 发送 `$EVH` 握手并持续周期性发送 `$EV1` 人数帧，脱离电脑独立 5 V 供电即可稳定运行。

---

## 2. 本目录文件说明（仅历史参考）

| 文件名 | 当时用途 | 现状 |
|---|---|---|
| `k230-sd-main.py` | 2026-11-24 临时验证 SD 卡 `/sdcard/main.py` 启动入口的最小包装脚本 | 已由 `ecovision/firmware/k230/main.py` 正式实现替代，**不可部署** |
| `k230-live-fallback.py` | 2026-11-24 电脑端通过 `canmv_backend_client` 拉起临时恢复会话的排障脚本 | 仅电脑端调试历史参考，**不可部署** |

> 当前最新项目状态与冻结基线，请以仓库根目录 `PROJECT_LOG.md` 与 `docs/DEMO-FREEZE-20261125.md` 为准。
