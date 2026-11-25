# 演示版本冻结 2026-11-25

适用于移远杯现场演示与答辩复现。冻结对象为设备仓 `main` 与配套 F413/F103/K230 构建产物；网页侧见 `smart-classroom-web` 的 `RELEASE-DEMO-20261125.md`。

## 1 源码

| 项 | 值 |
| --- | --- |
| 仓库 | Confidence-huang/smart-classroom-device |
| 分支 | main |
| 冻结提交 | `1880dbef3264c1d477c9bedde73e47363f9dd11e` |
| 提交标题 | feat(device): 接入模拟空调红外控制链路 (#16) |
| main CI | Device host tests `45611765504` success |
| 协议版本 | 设备 1.2.0（模拟空调），兼容 1.0.0/1.1.0 |

K230 冷启动修复以 PR #17（`fix/k230-cold-start-identity`，`2355cf0`）单独提交，未包含在本冻结提交内；合并后需按第 5 节重新复验再更新本文件。

## 2 固件与实测产物

| 目标 | 文件 | SHA256 | 来源 |
| --- | --- | --- | --- |
| F413 应用 | `F413-EcoVision-AC-AUTO-d22c778.hex` | `31257368c65ec6d5403b7e164c4446998754796ceec129317c216000fc242466` | 构建自 `d22c778`；`d22c778..1880dbe` 的 `EcoVision_F413ZH` 与 `contracts` 树无差异，仅新增 F103 与 CI |
| F103 OLED | `F103-AC-OLED-27c326d.hex` | `0692a43a3f2b15826ee768685be048199b1b46b2211ec7aabcb48bec127afb91` | 实测 `27c326d`，与 main 的 `f103-oled-spi`（SSD1306）一致 |
| K230 标识模块 | `ecovision/firmware/k230/runtime_identity.py` | `05fbb180090e6ffa7427d0343d5926e571cf190e38b97999cda4f77261b6c607`（板端读回） | 对应 PR #17；冻结提交内仍是旧实现 |
| 私有配置 | 1.0.0/1.1.0 配置 BIN | 不入库 | 按 `provision.py` 生成，现场配置区 `0x08160000` |

产物副本在本地 `outputs/F413模拟空调AUTO候选_d22c778/` 与 `outputs/F103模拟空调精简OLED候选_27c326d/`；仓库只登记哈希，不存二进制。

## 3 接线表

发射端与接收端只通过红外光耦合，电气上不共地。

| F413（CN8 内排） | ZJ-IR-V2 |
| --- | --- |
| CN8-10（PC12 / D47 / UART5_TX） | RXD |
| CN8-12（PD2 / D48 / UART5_RX） | TXD |
| CN8-7（3.3 V） | VCC |
| CN8-11（GND） | GND |

| 模块 | F103 |
| --- | --- |
| OLED SCL / SDA | PB6 / PB7（软件 SPI） |
| OLED RES / DC | PB8 / PB9 |
| 红外接收头 OUT | PA0（TIM2_CH1） |
| OLED 与接收头供电 | 兼容 3.3 V |
| 地 | F103 GND |
| SWD | PA13 / PA14 |

灯环、风扇（TB6612）、干簧管、BH1750 的既有接线不在本次冻结范围变更内，保持 D03/W02 已验收状态。依据文件：`ecovision/firmware/stm32/f103-oled-spi/README.md`、`ecovision/firmware/stm32/README.md`。

## 4 回退版本

| 回退目标 | 版本 | 说明 |
| --- | --- | --- |
| F413 | `9c4818c` 的 `EcoVision_F413ZH.hex`，SHA256 `163daf4fc253a4422c3b6f26a329327e16154514bb719e9d8249bbfe01c29120` | 无模拟空调的 1.0.0 冻结应用；配置文件 `outputs/D03评审修订候选_9c4818c/` |
| F103 | 同第 2 节 `27c326d` HEX | F103 固件在本次冻结前后一致，无需单独回退 |
| 私有配置 | 已备份的 1.0.0 配置 BIN | 应用回退时配置区可保持不变 |
| 网页 | `8a3b0c4` 对应的旧发布目录 | 部署时保留旧目录，停用新 unit 后重载即可 |

回退顺序：先停控制台下发（或切回旧网页），再烧 F413 1.0.0 HEX，最后按需恢复配置；F103 与接线不动。

## 5 复验状态

已通过：
- 网页开机 21 °C、调温、关机到 F103 OLED 的完整链路（用户现场确认）。
- F413 UART5 读 ZJ-IR-V2 地址 `00`，重试逻辑有效。
- K230 冷启动重复熵修复在原始实现上通过 USB 断电重上、独立 5 V、F413 有效人数帧与云端 PUBACK。

未在本冻结上复验：
- AUTO 门限矩阵（29/27/28 °C+70 %RH、300 s、0 人 30 s、失效 60 s、手动 120 s、B1 优先、强制 OFF）。
- 本冻结提交上的 K230 冷启动（TF 卡插紧、退出 IDE、独立 5 V）。
- 8 小时连续运行、物理拔线、断电持久性。

## 6 冻结后变更

任何影响 F413/F103 固件、K230 标识模块、接线或网页控制契约的改动都必须：新建提交、跑 PR CI、在本文件追加新版本行并打新 tag，不得覆盖 `demo-20261125`。
