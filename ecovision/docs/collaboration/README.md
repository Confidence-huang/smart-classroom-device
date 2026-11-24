# 队长接手与双仓库协作

## 当前仓库与协作规则（2026-11-22 更新）

- 最新分工：用户分别发起 D01/D02 PR；另一位仓库管理者审查、在PR中要求修改，验收后合并并接手第3轮。D01/D02完整验证前不开PR。最小AUTO已获用户同意提前到D02，见 [确认的设计](../plans/2026-11-22-local-auto-design.md)。
- [Git 纪律本地适配版](skills/git-discipline/SKILL.md)：已安装到本机 Codex 用户级 Skill 目录；保留上游来源与 MIT 授权，适配 Windows、既有会话授权和双仓库按功能 PR 流程。安装及 SHA256 核对见 [收据](evidence/git-skill-adaptation-20261122/installation.json)，不改变模块边界或已有分支。
- [PR 范围、优先级与分支保留协作文档](2026-11-22-pr-scope-and-branch-retention.md)：按实际提交区分可准备的 PR、待实现项及保留归档，后续审阅从这里开始。
- 网页仓库：[smart-classroom-web](https://github.com/Confidence-huang/smart-classroom-web)。当前网页地址：[智慧教室看板](https://confidence-huang.github.io/smart-classroom-web/)，由用户提供，本轮未执行网页功能验收。
- 设备仓库：[smart-classroom-device](https://github.com/Confidence-huang/smart-classroom-device)。两个仓库的 `chore/ecovision-checkpoint-20261121` 分支均保留，尚未合入远端 `main`。
- 本机克隆仍在 `docs/delivery/2026-11-21/web-repo` 与 `board-repo`；两者 `origin` 的 fetch/push 地址已更新为对应新仓库的 HTTPS `.git` 地址，fetch 成功。
- 每次修改前先读取各仓库 `AGENTS.md`、`PROJECT_LOG.md` 和 `README`，并核对 Git 根、`git status`、当前分支、HEAD 及远端地址；文件缺失时明确记录，不能假定存在。网页仓库当前本地与远端 `main` 均没有前两份文件，已读取现有 README；设备仓库三份文件均已读取，并区分本地日志与远端 `main` 日志。
- 不把整个 checkpoint 分支直接合入 `main`。先按功能审阅差异，选定范围后再准备 PR；PR 可由用户发起，无需总控代发。本轮未切换分支、合并、重置、提交、推送或创建 PR。
- fetch 后：网页本地 HEAD `19ed9e7`、远端 `main` `bc958b6`，相对领先 3 条、落后 2 条；设备本地 HEAD `5c74253`、远端 `main` `0f8b9d6`，相对领先 4 条。两个本地分支都名为 `main` 且工作区干净，HEAD 均等于各自远端 checkpoint；这是此前已有的本地状态，本轮原样保留，不能据此认为远端已合并。

下方为 2026-11-20 历史检查记录；旧名称、状态和技术边界保留溯源。仓库现名及后续协作方式以上述更新为准，设备后续验证另见日报和模块证据。

## 2026-11-20 历史记录

整理日期：2026-11-20。将用户提供的协作方案纳入当前集成工作区，供代码交接与后续开发查阅。

## 先读这几个入口

- [用户方案原文](sources/队长接手网页与双仓库Git协作方案_20261120.md)：逐字节归档，保留原始上下文与操作建议。
- [本机接手与验收清单](2026-11-20-handoff.md)：外部仓库、本机 C/HAL、接口差异与验收顺序。
- [Issue 模板](templates/issue.md)、[PR 模板](templates/pull-request.md)：复制到实际目标仓库使用，本目录本身不启用 GitHub 自动化。
- [当前板端显示实测](../../firmware/stm32/ENVIRONMENT-DISPLAY-RESULTS.md)、[K230 串口入口](../../firmware/k230/UART-STM32.md)：了解今天实际部署了什么。

## 仓库与代码定位

| 对象 | 职责 | 与当前目录的关系 |
|---|---|---|
| [project51-classroom-dashboard](https://github.com/Confidence-huang/project51-classroom-dashboard) | 外部 Vite 看板、Pages 发布产物及关联 API 服务源码 | 用户方案描述的网页仓库；不能把当前 `cloud/` 当成其克隆 |
| [project51-gen1pro-micropython](https://github.com/Confidence-huang/project51-gen1pro-micropython) | 外部 STM32 MicroPython、传感器/LCD/EC200U 上报 | 与本机 C/HAL 固件分开维护，MicroPython 包不能直接导入 C 固件 |
| [agent-git-discipline](https://github.com/Confidence-huang/agent-git-discipline) | 独立 Git Skill 来源 | 本次只归档方案，没有安装、配置或执行该 Skill |
| 当前集成目录 | K230、STM32 C/HAL、公共接口、本机 Node-RED、测试和硬件证据 | 本次检查根目录没有 `.git`；本次没有创建仓库、分支或远端 |

## 已知边界

当前 C 固件已实测接通 K230 UART，LCD 显示真实温湿度、光敏 ADC 和人数；当前 C 固件尚未恢复 EC200U 4G/MQTT。组员旧 MicroPython 上报成果、方案中公网 API 的可用记录和当前本地 C 固件必须分别核对，不能合并成整机端到端通过。

本文链接的线上状态、提交号、Pages 来源、权限、CORS 和 AI 状态均来自用户方案的 2026-11-20 检查快照，本次没有重新在线验证。执行实际任务时应重查。

本次仅汇总文档并加入索引/模板，没有刷板、安装依赖、访问付费 AI、创建 Issue/PR、push、部署 ECS 或修改 GitHub 设置。原文中的任务提示词和命令是参考材料，不是本次会话的新授权，也不作为本工作区的自动执行规则。
