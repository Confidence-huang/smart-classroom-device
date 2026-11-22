# EcoVision公共接口 1.0.0

状态：2026-11-12总控冻结的第一版开发基线；不是硬件认证结果。本文定义语义，contracts/*.schema.json定义形状，contracts/policy.json定义公共常量，发生冲突必须停止受影响集成并由总控修订。

## 1. 控制权与时间

仅STM32拥有输出真值。模式：AUTO、REMOTE、LOCAL_OFF。优先级沿用原型参考：本地关闭 > 实体按键切换 > 有效远程租期 > 自动。电气保护始终优先。长按≥2秒进入LOCAL_OFF并撤销租期；短按（消抖30ms后、释放且未达2秒）回AUTO并撤销租期。LOCAL_OFF只能由实体短按退出，远程AUTO也不得解除。

STM32每次启动生成不重复boot_id（8—32位小写十六进制，推荐持久启动计数），进入AUTO、输出0，不恢复旧手动租期。control_epoch为本次启动的uint32，每次实体按键操作递增，云端命令必须匹配，防止按键后在途旧消息接管；到上限须生成新boot_id并重新连接，不能无声回绕。

单调时间：uptime_ms，毫秒、从本次启动开始，JSON安全整数0..9007199254740991；计时差用单调时钟，不能因校时延长租期。墙钟：issued_at、ts、received_at为UTC Unix整数秒，未可信同步时上行ts/received_at=null、time_valid=false；拒绝新远程动作TIME_UNTRUSTED。上行云端记录自己的接收时间，不能凭历史设备ts判断实时在线。

AUTO：连续3个有效且新鲜的正人数帧确认有人；连续有效0人数且持续30秒确认无人（前一状态保持到确认）。每帧间隔不得≥5秒，无效帧或超时立即中断无人/进入连续确认，视觉状态UNKNOWN；重复/坏帧不刷新有效时间。确认有人LED=1；温度≥28℃连续5秒开风扇，≤27℃连续5秒关；中间保持。确认无人关闭两路。启动UNKNOWN时保持0；视觉距最近有效帧≥5秒或温度距最近有效样本≥10秒失效；AUTO失效先保持对应输出，连续关键故障≥60秒关闭两路。REMOTE由合法命令设置两路输出、hold_s到期回AUTO；感知故障不阻断手动，本地关闭仍最高。此故障关闭规则仅用于5V桌面原型。

## 2. K230 UART：ASCII + CRC + 显式会话

115200 baud、8N1、无流控；引脚、电平待实物核对，不提供通用针脚许可。K230只报视觉，每1秒一帧，无人也发，异常必须valid=0。

所有帧为ASCII，最大128字节（含CRLF），接收环缓冲至少512字节；CRC-16/CCITT-FALSE（poly=0x1021、init=0xFFFF、refin/refout=false、xorout=0），范围从`$`之后到`*`之前，四位大写十六进制，末尾严格`\r\n`。标准向量`123456789`→29B1。十进制数无符号、无空格、除0外不允许前导0。

```text
STM32 → K230  $EVQ,link_id*CCCC\r\n
K230 → STM32  $EVH,link_id,vision_boot_id*CCCC\r\n
K230 → STM32  $EV1,link_id,vision_boot_id,seq,uptime_ms,count,valid*CCCC\r\n
```

link_id、vision_boot_id均为16位小写十六进制，跨启动不得复用。STM32上电/视觉链失效生成新link_id，每1秒发EVQ直到EVH匹配；K230启动后等待EVQ，回应EVH后才发EV1。重复同一EVQ只能重发EVH，不能清零seq；同一link_id的重复EVH不重置接收水位。接收端仅在等待握手时接纳EVH，并锁定link_id及vision_boot_id；EV1必须两者匹配。握手及链路恢复不直接证明视觉有效；重新收集3帧/30秒确认。STM32不得自动切回旧会话。K230重启无新握手时旧帧失效，STM32在5秒超时后换新link_id，缓冲旧帧不能恢复感知。

seq为uint32，第一帧可为0；后续`delta=(new-old) mod 2^32`，仅0<delta<2^31为新帧，0重复，其余旧帧。uptime_ms为uint32毫秒，可回绕，与seq分开，不能用其判断UTC时效。count为0..20整数；valid为0或1；valid=0时count强制0占位且不得解释为无人。

接收按字节流解析，支持半包/粘包；遇新`$`重新起帧，累计超128字节、首字节起200ms未完整、非ASCII、错CRC、字段缺漏或超界则丢该帧并计错，继续寻找`$`。UART IDLE不是报文边界；无效、旧、重复帧均不刷新最近有效时刻。ACK不用于视觉上行。精确字节与错误向量见contracts/examples/uart.json。

## 3. MQTT与传输

应用基线MQTT 3.1.1、TLS验证服务器证书及域名；EC200U具体AT参数、TLS承载能力按实物固件核实。连接clean session=true；不恢复离线命令队列；保活30秒。主题不改原型候选名称，不增加版本层级：

| 主题 | 发布方→订阅方 | QoS | retain |
|---|---|---|---|
| ecovision/{device_id}/telemetry | STM32经EC200U→云端 | 1 | false |
| ecovision/{device_id}/cmd | 云端授权服务→终端 | 1 | false，收到保留命令拒绝 |
| ecovision/{device_id}/ack | 终端→云端 | 1 | false |
| ecovision/{device_id}/status | 终端/其遗嘱→云端 | 1 | true |

device_id为`[a-z0-9][a-z0-9_-]{0,31}`，例如ev001；载荷device_id必须匹配主题与凭据ACL。设备仅能发布自己的上行并订阅自己的cmd；云端服务可发白名单设备cmd。浏览器不持有Broker管理员凭据。role不从消息自报，鉴权由服务与Broker ACL提供。

载荷UTF-8 JSON对象，禁止重复键、NaN/Infinity、未知字段；最大2048字节。全部必填字段和枚举见schema；`v`固定`1.0.0`。遥测与status各有独立uint32序号；ACK不含seq，按cmd_id关联。云端按boot_id及模序去重，不能让旧消息倒退显示。每5秒遥测，状态变化额外触发；云端连续15秒无新鲜遥测标记STALE并禁用命令，不以retain状态证明在线。终端成功CONNACK后发布online状态；LWT在CONNECT预设offline、ts=null；正常断开主动发offline。status在每次CONNECT前预留连续两个序号n/n+1，online使用n，遗嘱或正常offline使用n+1，下次连接用n+2；同boot_id内重连不归零。这样遗嘱不会被online序号过滤掉，延迟旧遗嘱也不会覆盖新连接。遗嘱时间仅代表云端观察，旧boot_id遗嘱不能覆盖新会话；新boot_id通过新鲜遥测确认后锁定，旧会话不因status自行切回。

## 4. 遥测、有效性与单位

字段全表以telemetry.schema.json为准：v/device_id/boot_id/seq/uptime_ms/ts/time_valid标识样本；control_epoch标识人工控制代次；count/vision_valid/vision_age_ms、temp_c/temp_valid/temp_age_ms、humidity_pct/humidity_valid表示感知；occupancy为OCCUPIED/VACANT/UNKNOWN；mode、fan_out、led_out、hold_remaining_s为执行与租期；net_state为ONLINE/OFFLINE/CONNECTING；last_cmd_id为最近成功执行命令ID或null；faults为不重复故障码数组。

人数0..20（验收0..5）；温度摄氏度-40..85；湿度%RH 0..100。无效感知值一律null并valid=false，不能写0冒充有效；UART无效占位0在STM32转换为null。age_ms为距最近有效样本的本地毫秒数，从未有效为null。vision_valid=true必须age<5000，temp_valid=true必须age<10000；湿度有效性由模块采样确认，无温湿度模块时两者均无效。UNKNOWN不得等同无人。

fan_out/led_out为0/1电气驱动逻辑，不证明机械旋转。hold_remaining_s为整数0..300，非REMOTE必须0。time_valid必须与ts是否为null一致。faults枚举：VISION_FAULT、SENSOR_FAULT、NET_OFFLINE；断网本地快照使用OFFLINE及NET_OFFLINE，不能伪称当时成功发到Broker。本期不要求补传离线时序，重连先发当前状态；ACK/最近状态缓存在RAM最多32条，溢出先丢最旧，保留计错日志，不持久化旧命令。

## 5. 命令校验与幂等

command.schema.json全部字段必填：v、device_id、boot_id、control_epoch、cmd_id（1..64位字母/数字/下划线/连字符）、issued_at、ttl_s、mode、fan_out、led_out、hold_s。REMOTE须明确设置两路输出（绝对值，无toggle），hold_s=30..300，默认由云端填120；AUTO须fan_out=null、led_out=null、hold_s=0，由本地控制重算。云端不能发送LOCAL_OFF模式，远程全部关闭为REMOTE且两路0，可被本地接管。

ttl_s=1..30，默认15；接收时要求`issued_at <= now < issued_at+ttl_s`，未来时间拒绝FUTURE_COMMAND，无宽限。租期从首次实际执行的单调时刻起，不能把MQTT重投递当续租；重连不重置已存在租期。断网不自动结束现有租期，到期回AUTO。

校验顺序固定：大小/JSON/结构/版本 → 主题与授权/retain → device_id → boot_id → 幂等表 → 可信时间/时效 → control_epoch → LOCAL_OFF锁 → 参数/容量 → 执行与ACK。未知版本UNSUPPORTED_VERSION，结构无效INVALID_MESSAGE。无合法cmd_id、错误设备或未授权消息仅记录/丢弃，避免应答到其他设备；可关联的合法身份消息返回拒绝ACK。retain=true返回RETAINED_COMMAND。

当前boot_id内按cmd_id保存已执行命令的规范化内容（键序无关，字段值全部参与）及首次APPLIED ACK。相同ID同内容只重发原ACK，含原received_at/uptime_ms/输出快照，即使后来过期、按键或状态改变也不重新执行；云端实时输出以新遥测为准。同ID不同内容拒绝CMD_ID_CONFLICT并保留首次记录。保留期至少到该ID命令的issued_at+ttl_s；有效幂等条目不得提前驱逐，容量32满则新命令BUSY，不执行。拒绝不占幂等表，相同消息以后重试需重新校验，因此环境恢复后可能被接受；云端命令记录保留每次拒绝，不把拒绝当成成功缓存。过期清理后旧命令仍因时效/epoch被拒；不缓存未来命令造成无限占位。重启清空缓存，但旧boot_id拒绝。至少保留到TTL后可继续保留最近记录；若已驱逐，过期重试返回EXPIRED而非原ACK，两种情况都不得重新动作。

## 6. ACK与页面

ack.schema.json字段：v/device_id/boot_id/cmd_id/status/reason/received_at/uptime_ms/control_epoch/mode/fan_out/led_out/hold_remaining_s。status仅APPLIED或REJECTED；reason枚举见schema，APPLIED必须OK，REJECTED不能OK。APPLIED表示校验成功并已写入输出/切换模式，非仅收到消息、非物理风扇反馈。拒绝不改变输出/模式/租期；ACK中的状态为处理时快照。

页面点击只显示PENDING，收到可关联APPLIED ACK后显示已确认，随后遥测更新当前状态；10秒无ACK显示TIMEOUT/结果未知（不是断言未执行）。迟到ACK可更新命令历史但不覆盖较新实时状态。云端不自动新建cmd_id重试超时动作；如重发必须原ID原内容且仍在TTL内。ACK丢失通过同ID重发恢复；新操作生成新ID。

## 7. 版本与变更

冻结号1.0.0，UART EV1是本项目首版完整格式，不能接收旧参考五字段。禁止模块自行增加/改名字段、主题、时效或控制优先级。新增必填、改变语义/单位/主题、改变UART布局均为破坏性变更升主版本；增加可选字段升次版本并先定义协商，本版仍严格拒绝未知字段；纯解释勘误升补丁。模块提交“问题、受影响样例、建议、兼容性、验证”，总控同时更新本文、schema、policy、样例、测试、CHANGELOG后统一宣布生效。11-25后只允许必要缺陷修正并重跑受影响测试。
