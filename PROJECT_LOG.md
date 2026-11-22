# Gen1 Pro MicroPython 项目日志

## 当前快照

- 2026-11-22 D02审查修复：原PR #2分支加入内部无效帧计数，控制回归RED/GREEN、视觉C、17项契约和32项K230主机检查通过。D01修复dbbb873已上传并回PR，等待维护者合入main；D02待同步main后生成包含两处修复的同一HEX并做用户新授权短测。当前修改后源码尚无新HEX，不代队友合并。

以下为原MicroPython截至11月18日的历史快照，不代表本轮C/HAL状态：

- 项目：NUCLEO-F413ZH + UniKnect Gen1 Pro 的板载环境监测；K230 人数未接入。
- 固件：官方 F413ZH MicroPython ELF 候选保存在上级项目 `work/`，本仓库不含固件。板端 COM15 已返回 `micropython` 1.27.0 preview，机器标识 `NUCLEO-F413ZH with STM32F413`；所选 ELF 文件名在原烧录截图中未完整显示。
- 代码：`src/main.py` 周期采集并调度 LCD 与 4G 上报；`src/environment.py` 读取 AHT20/PC5；`src/classroom.py` 定义 JSON 契约，光敏只报 ADC 原码。
- 电脑环境：日常使用 VS Code 工作区、Python 3.12、`uv` 和 `mpremote`；Thonny 4.1.7 已于 2026-11-18 从本机卸载。STM32CubeProgrammer 保留用于固件烧录与校验。
- 验证：主机四项单元测试及七份脚本语法检查通过；实板 I2C 扫到 0x38，AHT20/PC5 有实时读数。2026-11-18 重烧官方 F413ZH MicroPython 固件后已恢复 `/flash` 全部公开模块、EMQX 根 CA、交互写入的 `settings.py` 及 `main.py`（SHA256 全一致）；软复位与物理按 NUCLEO `RESET` 均已实测自启，用户目视确认 LCD 显示 `Temp: 27.1 C`、`RH: 28.9 %`、`Light ADC: 18292`、`People: --` 与 `4G MQTT: sent`，串口连续输出 `PUBLISHED`。此前 11 月 16 日已验证本机 SQLite 历史订阅与公开网页 WSS 实时接收；每分钟 NTP 校时后采集/接收差约 3–9 秒。
- Git：第 12 次 git `a19d72523e16d7f63a667efa33a903871be04184` 为本次更新前的检查点；当前编号以 Git 现查为准。
- 安全：设备账号与根 CA 仅存本地忽略文件；不复制组长程序。
- 下一步：为长期历史准备可访问的 HTTPS 服务；与队长接入 K230 人数和光照标定。断电重启、断网重连需独立实测。不要运行带示例 MQTT 凭据的 `examples/application.py`。

## 关键文件索引

- `README.md`：环境、配置和实板验证顺序。
- `src/settings.example.py`：无秘密配置模板。
- `provision.py`：交互输入密码并直接写板端运行配置，不落电脑明文文件。
- `src/classroom.py`：模拟载荷及编码。
- `src/environment.py`：板载 AHT20 与 PC5 光敏采样。
- `src/screen.py`：LCD 四项布局。
- `src/uplink.py`：4G、NTP、TLS 和 MQTT。
- `src/main.py`：板端运行入口。
- `src/cellular_check.py`：不使用账号的 4G 附着与 DNS 最小测试。
- `tests/test_payload.py`：电脑端契约测试。
- `移远杯-MicroPython.code-workspace`：VS Code 双根工作区与语法、测试、串口、REPL 任务。
- `check_syntax.py`：不运行板端代码的主机语法检查。

## 近期任务记录

- 2026-11-22：同步共享PROGRESS与首页入口，更新灯扇动作用户反馈及实际关闭、供电、AUTO和远控等剩余事项；合并仅涉及进度文档，固件与契约未改。

- 2026-11-22：按最新“直接开”授权准备上传选定功能分支及证据，创建D01→main、D02→D01的两个PR供队友审查；不新增软件/硬件复验、不自动合并。最终链接及远端SHA见总控日报。

- 2026-11-22 本次实板复验：确认模型/源资产、纠正UART内侧引脚接线，保留net8一小时670/720与net9失败窗口；恢复候选和最小AUTO软件/构建通过，实测B1与正式遥测对应。用户放宽人数精度；负载缺席、空场景误检和最终固件稳定性未完成，不开PR。下一步先解决主UART稳定性并补实际负载，最终证据门槛通过后由用户开PR。

- 2026-11-22：按先完整验证后 PR 的要求准备 D01 独立 worktree，选择 1,586 个基础文件并修正入口/忽略规则；58 个工程引用齐全，主机检查与 Keil 0 错误/456 告警，HEX 与历史已烧录镜像同哈希；公共契约 16 通过/1 跳过，历史 44/44/20 复核。实板缺席，未烧录、推送或开 PR，下一步接入硬件补当前上报与 1 小时证据。

- 2026-11-18：官方固件重烧后完成板端恢复与自启验收：恢复 `classroom.py`、`environment.py`、`screen.py`、`uplink.py`、`mqtt_ca.pem` 与 `main.py` 至 `/flash`（逐文件 SHA256 与本地一致），用户交互写入 `/flash/settings.py`（密码未落盘）。软复位后连续打印 `PUBLISHED`；用户目视确认 LCD 实显（`Temp: 27.1 C`、`RH: 28.9 %`、`Light ADC: 18292`、`4G MQTT: sent`），并实测按 NUCLEO 物理 `RESET` 后先白屏再自动恢复显示与上报。主机 4 项单测与 7 份脚本语法检查全部通过，全历史安全扫描无凭据或无关代码；准备绑定并推送至公开 GitHub 远端。
- 2026-11-18：核对本机 Thonny 4.1.7 仍有卸载项与程序文件，完整备份并逐文件 SHA256 校验后运行原安装目录的卸载程序；退出码 0，卸载项与安装目录均已消失。README 改为 VS Code/uv/mpremote 操作与测试命令，四项数据契约测试通过；未改板端固件或设备文件。
- 2026-11-16：公开 GitHub Pages 实际订阅同一 EMQX 主题，显示实测温湿度/ADC、人数未接入；发现板端 `captured_at` 比服务接收时间落后约 8 分钟，网页标记“数据已过期”。`src/main.py` 增加每 60 秒 NTP 校时，主机四项测试和语法检查通过；新入口上传 COM15 设备并读回 SHA256 一致、软复位自启后持续上报，历史 API 的采集/接收差约 3–9 秒。公开网页重新连接后显示“实时数据”及实测 4G 链路状态。多条历史仍只在本机 SQLite。
- 2026-11-16：新增无密码落盘的 `provision.py`，板端写入 `settings.py` 并哈希比对；含密码配置不入电脑项目和 Git。临时运行 `src/main.py` 连续打印同一设备的 `PUBLISHED`，本机历史服务 `STORED` 相同序号，API 最新记录 `simulated:false`、温湿度/ADC 实测、人数和 lx 为 null。原厂占位 `main.py` 已备份到 E 盘后上传新脚本，软复位后历史服务继续收到新序号。公开网页浏览器端的逐值实收待核对；本机历史尚非云端历史库。
- 2026-11-16：官方硬件指导确认板载 AHT20-F、GL5528 光敏与 S502 MCU/模组切换；实板 I2C1 扫到 0x38，AHT20 约 25.2 °C/49.36%，PC5 ADC 约 22917。新增实测环境载荷：`simulated:false`、`people_count:null`、`light_lux:null`、`light_raw_u16`；LCD 用同次采样显示 ADC，用户目视确认清晰。EMQX 公共根 CA 1294 字节经板端 SHA256 校验，IPv4 TCP 和 CA 校验 TLS 握手成功。MQTT 凭据、QoS 1 发布和云端订阅未验证。
- 2026-11-16：板端 RTC 初始读数为 2015 年。`src/uplink.py` 临时运行 NTP 首次在厂商 `usocket.recv` 报 `UnicodeError`，`recvfrom` 同样失败；改用 `readinto(bytearray(48))` 后，板端返回 `NTP_UTC_OK (2026, 11, 16, 7, 0, 35)`，再次读取 RTC 为 2026-11-16 07:00:40 UTC。NTP 包校验和 RTC 写入通过；未使用 MQTT 凭据、未发布消息。下一步 TLS/MQTT 配置与设备自主发布。
- 2026-11-16：增加 `src/cellular_check.py`，依照官方 Network 示例对 EC200U 执行初始化、USIM 检查、4G 附着及 `example.com` DNS 查询。首次发现固件不提供标准名 `socket`，按官方 `modules/socket/udp.py` 用 `usocket as socket` 修正检查脚本及 `src/uplink.py`；实板重试返回 `CELLULAR_ATTACH_OK`、`CELLULAR_DNS_OK`，模组底层有 IPv6 `inet_pton` 报错但 IPv4 DNS 查询完成。未验证 NTP、TLS/MQTT 或网页。
- 2026-11-16：用户确认官方 LCD 最小例程运行正常。发现该无限循环任务仍占用 COM15，精确停止其 `uv/mpremote/python` 进程后，本工程 `src/screen.py` 增加单次模拟布局入口；主机语法检查、3 项既有单测通过，`mpremote run src/screen.py` 返回 `SCREEN_DEMO_DRAWN people=3 temp=24.3 humidity=47 light=320`，用户目视确认四项清晰且无重叠。没有上传脚本、写入固件或连接网络。
- 2026-11-16：用户 VS Code 截图证实 `mpremote` 经 COM15 返回 MicroPython 1.27.0 preview 与 NUCLEO-F413ZH 机器标识；本机只读 `fs ls` 及 `os.listdir('/flash')` 见 `boot.py`、`main.py`，`st7735.LCD`、`quectel.Network`、`umqtt.robust.MQTTClient`、`tls` 导入均成功。EC200U AT COM18 当前也已枚举。未运行 LCD 例程、上传文件或联网发布，下一步屏幕最小验收。
- 2026-11-16：为本工程建立 VS Code 专用工作区和配置文件，接入 Python 3.12、锁定 mpremote 1.29.0，配置语法检查、数据测试及板端串口任务。电脑端 3 项测试和两份脚本语法检查通过；Python/Pylance 在专用配置文件安装并列出。未执行实板串口任务或上传，REPL 状态待验。
- 2026-11-15：创建独立源码工程，记录第 1 次 git；电脑端三项载荷测试与语法编译通过。板端和云端验收尚未执行。

- 2026-11-22 PR #2审查7080789667：在原分支应用五文件补丁，先仅加入回归和计数声明，旧控制逻辑在无效帧被覆盖场景提前OCCUPIED/亮灯的断言失败；补齐生产代码后控制/视觉C通过，17项契约和32项K230主机检查通过。公共1.0.0字段/主题未改。待D01复审合入main再同步、重新Keil Build、记录SHA/HEX哈希并做指定短测；旧实板记录不改写。
