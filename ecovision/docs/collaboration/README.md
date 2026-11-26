# 队长接手与双仓库协作（历史归档）

> **历史归档声明**：本文为 2026-11-20 至 2026-11-22 的阶段性协作记录归档；其中涉及的旧仓库状态、分支领先/落后计数已过时，**当前最新项目状态与冻结基线以根目录 `PROJECT_LOG.md` 为准**。

## 2026-11-20 历史记录

整理日期：2026-11-20。将用户提供的协作方案纳入当前集成工作区，供代码交接与后续开发查阅。

## 先读这几个入口

- 用户方案原文（未随本 PR 提供的归档引用 `sources/队长接手网页与双仓库Git协作方案_20261120.md`）：逐字节归档，保留原始上下文与操作建议。
- [本机接手与验收清单](2026-11-20-handoff.md)：外部仓库、本机 C/HAL、接口差异与验收顺序。
- Issue 模板（未随本 PR 提供的归档引用 `templates/issue.md`）、PR 模板（未随本 PR 提供的归档引用 `templates/pull-request.md`）：复制到实际目标仓库使用，本目录本身不启用 GitHub 自动化。
- 当前板端显示实测（未随本 PR 提供的归档引用 `../../firmware/stm32/ENVIRONMENT-DISPLAY-RESULTS.md`）、[K230 串口入口](../../firmware/k230/UART-STM32.md)：了解今天实际部署了什么。

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
