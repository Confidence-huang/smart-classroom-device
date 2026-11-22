# Smart Classroom 设备工程

本轮 D01 引入 `ecovision/firmware/stm32/EcoVision_F413ZH/` 下的 C/HAL 工程，保留根 `src/` 的原 MicroPython 基线。两套固件独立，禁止为运行旧示例直接覆盖当前实板。

- C/HAL 构建、配置区、软件验证与未完成项：[ecovision/README.md](ecovision/README.md)。
- 本轮软件结果与硬件验证门槛：[D01 验证记录](ecovision/validation/D01.md)。
- 当前网页：https://confidence-huang.github.io/smart-classroom-web/ 。网页存在不代表收到新鲜实测。
- 2026-11-22 已进行实板构建/烧录/只读回及真实4G短测；D01网络恢复候选仍复现超时，一小时稳定性未通过。现场目前为D02 local-auto-20261122.2，灯/风扇未接入，实际动作未测；未推送或开PR。见 ecovision/validation/D01-RECOVERY.md。

当前源码候选版本与HEX哈希见 [CANDIDATE.json](ecovision/validation/CANDIDATE.json)。其中硬件完整验收为false；旧版本实板记录不能自动迁移到新候选。

## 以下为原 MicroPython 基线说明（保留历史）

以下验收记录针对原 MicroPython 固件及当时设备配置，不代表当前 C/HAL 新增硬件通过。

### Gen1 Pro 智慧教室板端 MicroPython

本仓库保存新写的板端环境监测程序。目标硬件为 NUCLEO-F413ZH、UniKnect Gen1 Pro EC200U 扩展板及官方 160×128 ST7735S LCD。板载 AHT20 温湿度与 PC5 光敏 ADC 已实测可读；K230 尚未接入，人数明确留空。程序不会上报 CO₂。组长的 C 工程、固件和相关文件均未复制到本仓库。

## 文件与数据流

`src/main.py` 触发周期采样；`src/environment.py` 从板载传感器采集；`src/classroom.py` 把同一次采样编码为 JSON；`src/screen.py` 在 LCD 同屏显示；`src/uplink.py` 经 EC200U/4G、NTP、TLS 与 MQTT 发布。`src/settings.example.py` 是不含秘密的配置模板。设备 `/flash` 中的 `settings.py` 和根 CA 必须独立放置，且不得提交 Git。

当前板端配置可用 `uv run python provision.py --port COM15 --user classroom_test` 交互写入；串口号须以设备管理器现场显示为准。密码由终端隐藏输入，脚本在电脑内存组装配置并直接写入板端 `/flash/settings.py`，不会在电脑创建含密码文件；已有板端配置时拒绝覆盖。设备闪存仍可被有实物访问权的人读取，长期运行应改用仅允许发布该主题的专用账号，并在账号泄露时轮换密码。

发布到 `classroom/room01/telemetry`，MQTT 3.1.1 over TLS，端口 8883，QoS 1。载荷示例：

```json
{"schema_version":1,"type":"telemetry","source":"stm32_micropython_aht20_ldr","simulated":false,"device_id":"f413zh-gen1pro-01","room_id":"room01","seq":848127600,"captured_at":"2026-11-16T07:00:00Z","people_count":null,"temperature_c":25.3,"humidity_pct":49.7,"light_lux":null,"light_raw_u16":23029}
```

`seq` 为已校准 UTC 时钟的秒数（本固件的 epoch 是 2000 年），每次发布间隔至少 2 秒以免重复。板载光敏只得到 ADC 原码，没有适用当前板的 lux 标定曲线，所以 `light_lux` 必须为 `null`；网页和 LCD 显示“ADC”。程序每 60 秒重新从 NTP 同步 RTC，修正实测发现的板端时钟漂移；若同步失败、CA 缺失或证书校验失败，LCD 仍更新实测值，但不发布带假时间的消息。串口打印 `UPLINK_ERROR`，不会打印密码。QoS 1 发布使用 retain，EMQX 保存该主题**最新一条**；完整历史由独立订阅服务存储。

## 电脑环境与静态验证

- VS Code 1.146.0 可作为日常主 IDE。打开本仓库的 `移远杯-MicroPython.code-workspace`，选择专用配置文件“移远杯-MicroPython”。左侧同时显示本工程和官方仓库；官方例程只作参考，不需要另装仓库中推荐的 Thonny 才能编辑本工程。
- 本工程用 `uv` 固定 Python 3.12 环境，`uv.lock` 当前锁定 `mpremote` 1.29.0。首次打开先在本目录运行 `uv sync`。VS Code 安装 Python 与 Pylance 扩展后，解释器选本目录 `.venv/Scripts/python.exe`。
- 在 VS Code 中打开一个 `.py`，按 **Ctrl+Shift+B** 执行“MicroPython：检查当前文件语法”；也可在“终端 → 运行任务”中执行“电脑：运行数据契约测试”。这些任务检查脚本，不会连接或改写开发板。官方例程同样可以用语法检查任务检查。
- 需要实板时，先退出 STM32CubeProgrammer 的连接、按 NUCLEO 的 RESET，并确认设备管理器中的 ST-LINK 虚拟串口号。在“终端 → 运行任务”依次执行“板端：列出串口”“板端：读取 MicroPython 版本”“板端：列出文件”。只有得到版本和文件列表后才运行当前脚本或上传；默认 COM15 只是上次发现值，每次以当前枚举为准。
- “板端：运行当前脚本（不上传）”通过串口临时运行打开的文件；“板端：上传当前脚本（写入设备）”会提示设备目标路径，须先确认 `/flash` 存在和目标文件名，避免覆盖板上自启 `main.py`。REPL 任务可在 VS Code 内交互。VS Code 终端若报端口被占用，先断开 CubeProgrammer 或其他串口终端。
- 官方 LCD 最小例程是持续循环，测试后必须先停止其 VS Code 任务，释放 ST-LINK 串口。随后打开本工程 `src/screen.py`，运行“板端：运行当前脚本（不上传）”；它只画一次固定的四项模拟值并打印 `SCREEN_DEMO_DRAWN`，不会连接 4G、MQTT 或写入板端文件。首次实板测试中，用户已目视确认四项文字清晰且无重叠。
- 打开 `src/cellular_check.py` 并运行同一临时任务，可按官方 `Network.init → query_usim → attach → deinit` 顺序检查 EC200U，随后只解析 `example.com` 域名，不会访问网页或使用 MQTT 凭据。`CELLULAR_ATTACH_OK` 与 `CELLULAR_DNS_OK` 分别表示附着和 DNS 查询通过；模组的底层调试日志可能包含 IPv6 解析报错，仍须以脚本标记和 IPv4 结果综合判断。
- 打开 `src/uplink.py` 并临时运行，会附着 4G、向 `pool.ntp.org` 获取 UTC、校准 RTC，然后释放网络对象；不会读取 `settings.py` 或连接 MQTT。`NTP_UTC_OK` 表示收到有效 NTP 包并设置 RTC。本固件的 `usocket.recv/recvfrom` 读取二进制 NTP 包会抛 `UnicodeError`，因此实现使用 `readinto`，此路径已在板端实测通过。
- 板端 `I2C(1)` 扫描到 0x38，官方 `ahtx0.AHT20` 返回温湿度；PC5 ADC `read_u16()` 返回光敏原码。`src/environment.py` 拒绝缺失传感器或越界数据，避免静默上报模拟值。`mpremote mount src` 临时运行后，实测 LCD 同时显示人数待接入、温度、湿度与 Light ADC，用户目视确认四行清晰。已有 EMQX 根 CA 已按 SHA256 复制至板端 `/flash/mqtt_ca.pem`，板端 IPv4 TCP 和证书校验 TLS 握手通过；MQTT 登录与发布仍须独立验收。
- 官方 `.py` 例程由板上 MicroPython 解释器执行，电脑上的语法检查不是 STM32 固件编译，也不能验证厂商模块是否存在。STM32CubeProgrammer 仍负责官方 ELF 固件的烧录与校验。暂不生成 `.mpy`，需先读取板端固件的 `sys.implementation._mpy` 并确认交叉编译器版本。
- Windows 11 x64；日常编辑、语法检查、数据测试与板端 REPL 使用 VS Code 工作区、Python 3.12、`uv` 和 `mpremote`。端口选择现场实际枚举的 **STMicroelectronics STLink Virtual COM Port**。
- STM32CubeProgrammer 2.23.0 Windows x64 用于将官方 F413ZH ELF 烧录并 Verify。固件候选在项目的 `work/UniKnect_官方固件候选_v1.2.0/`，SHA256 见上级项目实施方案。烧录须先在 GUI 确认目标芯片确为 F413ZH；本仓库不含固件副本。
- 在本目录运行 `uv run python -m unittest discover -s tests -v`。这仅验证 JSON 契约，不代表板端联网通过。

## 板端分阶段操作

1. 接好 ST-LINK USB、EC200U 扩展板电源、MAIN 蜂窝天线并打开 S201；S501 选已开通的 eSIM。电脑端分别确认 ST-LINK VCP 与 EC200U 端口，不能依赖旧 COM 编号。
2. 用户已提供 STM32CubeProgrammer 显示“下载验证成功”的截图；所选文件名在截图中未完整显示，故仍须用 VS Code 的版本与文件列表任务确认 MicroPython 提示符和 `/flash`。不要执行全片擦除、解锁、Option Bytes 更改或 ST-LINK 固件升级。
3. 实板已验证 `from st7735 import LCD`、`from quectel import Network`、`from umqtt.robust import MQTTClient`、`import tls` 可以导入；官方 LCD 例程及本工程实测布局已目视验收。4G 附着、DNS、UTC/NTP、EMQX CA 校验 TLS 握手及 MQTT QoS 1 发布均通过。
4. `src/settings.example.py` 只作无秘密模板。用 `provision.py` 交互输入 MQTT 密码，直接写设备 `/flash/settings.py`，不在电脑项目创建含密码的配置文件。与当前 EMQX 证书链匹配的根 CA 已单独核对并上传为 `/flash/mqtt_ca.pem`；更换云实例或证书链时须重新核对。
5. 将 `classroom.py`、`environment.py`、`screen.py`、`uplink.py` 和根 CA 复制到设备 `/flash`；先临时运行 `src/main.py` 核对 LCD 与 `PUBLISHED device_id=... seq=...`，再备份旧 `/flash/main.py` 并上传新入口实现自启。本次已按该顺序完成，更新入口后板端哈希与本地一致；若 `mpremote` 无法进入 raw REPL，先检查串口占用、端口号及板端固件状态。
6. 在网页设置中连接同一 EMQX 实例，核对 `simulated:false`、设备 ID、序号、UTC、温湿度和光敏 ADC。公开网页已收到实测消息且显示“实时数据”。断网重连和断电重启仍需分别复验；电脑测试脚本或 EMQX 控制台手动发布，不算板端通过。

## 当前验收边界

本次已在实板核对 LCD、传感器、4G、NTP、TLS 与板端 MQTT 周期发布，并由 EMQX 订阅端把相同设备 ID/序号写入本机 SQLite。公开 GitHub Pages 已在浏览器实收温湿度/ADC 并显示实时状态；每次新浏览器会话仍须输入 MQTT 凭据，源码与静态站点不保存密码。当前 SQLite 在本机，EMQX retained 仅保存最新快照；多条历史若要让所有手机长期查看，仍需部署常在线 HTTPS 历史服务与持久磁盘。

上游参考：移远官方 `https://github.com/quectel-develop/uniknect-micropython` 的 LCD、Network、`umqtt.robust`、`tls.SSLContext` 示例；源码未复制其测试账号或固定密码。
