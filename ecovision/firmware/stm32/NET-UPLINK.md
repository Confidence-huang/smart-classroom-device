# 当前C固件真实4G上报：net-20261121.8

2026-11-21，用户授权直接修改、构建及实板联调。当前NUCLEO-F413ZH已烧录本版本；EC200U为EC200UCNAAR03A17M08，电脑USB AT为本次枚举的COM25，STM32日志COM4。COM号应现场识别，不固定照抄。

## 已完成

- 当前C/HAL通过USART6 PG14/PG9与EC200U主串口通信，真实SIM READY、EPS注册和数据附着已查询。用户确认J308为Nucleo-144档1–3、2–4；波特率115200、无流控实测可用。
- 独立networkTask处理AT、超时、2/5/10/30/60秒加0～20%抖动退避；UART6 IRQ只收字节和记错，不在中断联网。日志、视觉与原有本地命令继续运行。
- 修复异步通知仍发往USB AT的问题，启动时明确路由到uart1。以运营商已同步的QLTS本地时间及明确时区换算UTC，未取得可信网络时间时尝试NTP，不拿未验证RTC冒充可信时间。
- 台架曾出现QMTOPEN 0,4域名解析失败，已记录失败与恢复。现有PDP原为IPv4/IPv6双栈，当前C程序保留原APN/认证配置并采用IPv4数据上下文，重试后持续发布；没有把主机解析出的IP写死到固件。
- TLS使用恢复的原CA、TLS1.2、server authentication、SNI；不忽略证书时间、签名或名称检查。具体实现依据移远[MQTT应用说明](https://www.quectel.com/content/uploads/2024/02/Quectel_EC200UEG915U_Series_MQTT_Application_Note_V1.1.pdf)、[SSL应用说明](https://quectel.com/content/uploads/2024/02/Quectel_EC200UEG91xU_Series_SSL_Application_Note_V1.2.pdf)与[AT手册](https://quectel.com/content/uploads/2024/02/Quectel_EC200UEG91xUEG915G_Series-AT-Commands-Manual_V1.1.pdf)，实际能力另经本模组查询核对；错误CA/域名的完整负向实测仍未执行。
- 正式1.0.0状态/LWT与5秒遥测：QoS1，telemetry retain=false；无有效人数时count=null、vision_valid=false。LED/风扇记录电气输出逻辑，不当作机械反馈；occupancy保持UNKNOWN，现有完整AUTO/远控尚未验收。
- 为原看板保留**私有配置明确启用**的旧主题兼容副本，原classroom主题、旧字段及2000年epoch序号不变，source改为实际C来源stm32_c_hal_ec200u、simulated=false。兼容副本retain=true沿用旧方案，正式遥测仍retain=false。缺可信时间、有效温湿度或ADC时不伪造旧格式数据；正式遥测仍可报告无效值。没有新增或修改公共1.0.0字段/主题/版本。
- LCD网络行改为实际任务状态，ONLINE来自MQTT连接及在线状态发布确认。输送层PUBACK是Broker确认，**不是远控业务ACK或实际风扇反馈**。

## 现在怎么使用

1. 保持NUCLEO与扩展板供电、LTE天线及SIM有效，沿用当前已确认的跳帽。当前设备配置已独立写入Flash，不需要电脑运行转发脚本。电脑订阅程序仅用于验证，没有发布模拟数据。
2. 上电等待NET:ONLINE；首次入网或DNS故障可能经历退避。串口可见[NET] PUBACK与LEGACY PUBACK。若长期OFFLINE，按日志阶段查AT、SIM、REGISTER、PDP、TIME_SYNC、TLS_CONFIG、MQTT_CONNECT，不通过关闭TLS验证绕过。
3. 使用[原公开看板](https://confidence-huang.github.io/smart-classroom-web/)查看数据。已有HTTPS API实际返回200、broker_connected=true，最近序号与C实测关联；本轮IAB打开页面被连接关闭，未独立完成浏览器视觉验收。人数仍为未知，K230需要运行正式UART入口；BH1750无有效读数时显示LUX:--，ADC不能标成lux。
4. 不要为测试单一外设刷回旧BH1750/串口/风扇HEX而丢失网络功能。后续从当前源码增量修改，并保留独立配置区。

## 配置与恢复

设备配置位于Flash 0x08160000起的独立区（F413 1.5MB器件的最后128KB扇区），应用链接范围限制到0x00160000。配置含长度、CRC与合法性检查；CRC不是加密或身份认证，实物访问者仍可能读取Flash。

本机firmware/stm32/.private保存恢复的设置、CA、独立配置数据及烧录前完整Flash；该目录及原整片Flash备份已在Git/导出规则中排除。实际值未写进C源码、应用HEX、公开日志或聊天。原备份恢复仅针对用户已使用的项目配置，不创建新身份或扩大权限。

仅在需要重新生成本机私有配置数据时运行：

```powershell
python -B firmware/stm32/tests/provision_network.py --legacy-compat
```

这条命令不烧录，输出只在.private内。应用HEX与私有network-config.bin分别写入及校验；禁止全片擦除后忘记恢复配置，禁止把包含配置区的整片Flash当公开固件上传。当前原镜像完整备份1572864字节已核对，正常使用无需反复恢复或擦除。

## 文件与验证

- app_net.c/h：独立任务、UART6收发、入网/TLS/MQTT与重连。
- net_codec.c/h：AT流解析、CRC/私有配置校验、UTC转换、APN保持、正式遥测及旧格式兼容编码。
- main/中断/MSP/.ioc/Keil：USART6、网络任务、24KB RTOS堆及链接区隔离；保留其他UART回调分发。原输出模块新增只读档位查询，视觉新增只读有效快照。
- net-evidence/2026-11-21/EcoVision-net-20261121.8.hex：实际已烧录镜像，SHA256 dcf65b3e22d160657b3b9e38169bb2dbf588ba1b1ca304f0290f786cc7387569。
- keil-final.log：0错误456告警，Code59334、RO5438、RW36、ZI94068。首次沙箱构建armasm 0x26F失败原日志保留；沙箱外构建成功，未把失败记为通过。告警尚未全部清理。
- host-net-final.log：AT分帧/溢出、URC数值、UTC/日历、保留APN、配置CRC/边界、有效性及兼容编码通过；已有风扇、视觉协议和BH1750/LCD软件回归通过，公共契约17项通过。
- final-live-events.json/final-live.log、reset-upload-result.json：本版本首次启动与再次STM32复位后，均恢复至少3组正式/兼容PUBACK。启动空闲堆12144B、视觉剩余栈2468B只代表本次短测。
- subscriber.json、api-final.json与docs/daily/evidence/2026-11-21-real-uplink-review.json：Broker正式schema、retain、两类消息及云端同序号/时间/传感值关联。早期失败、旧保留消息与重连过程保留，不混作新鲜数据。

复核：

```powershell
& firmware/stm32/tests/test_net.ps1
python -B -m unittest discover -s tests/integration -p "test_*.py" -v
python -B tests/integration/review_uplink_evidence.py --evidence firmware/stm32/net-evidence/2026-11-21 --report docs/daily/evidence/2026-11-21-real-uplink-review.json
```

代码、主机软件验证与短时真实4G/云端关联已完成；连续1小时完整率、8小时综合、模组整机断电恢复、完整TLS负向测试、K230精度、AUTO及远控业务ACK仍未完成。没有购买服务、调用付费AI或新建公开部署。
