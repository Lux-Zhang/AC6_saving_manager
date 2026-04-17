# AC6 Data Manager Native C++ PRD

- 文档状态：Approved for Execution
- 版本：v1.0-native-cpp
- 最后更新：2026-04-14
- 技术路线：`C++20 + Qt 6 Widgets`
- 关联技术方案：`docs/ac6-data-manager.native-cpp.technical-design.md`
- 关联验收基线：`docs/appendices/ac6-data-manager.native-cpp.acceptance-baseline.md`

## 1. 目标

把 AC6 Data Manager 的长期交付主线切换为 `C/C++` 原生实现，面向 Windows x64 正式发布与长期维护。当前阶段只覆盖 `Emblem stable lane`，并重做成普通用户可直接测试的桌面产品。

本阶段必须同时解决三类问题：
1. 当前 Python 线“打开真实存档失败”的产品风险。
2. 当前 GUI 暴露 `dry-run / apply / rollback / 审计` 等开发者语义，不适合普通用户测试。
3. 当前交付包体积偏大，需要在完整保留 WitchyBND 依赖不变的前提下控制到更合理范围。

## 2. 已确认决策

### 2.1 方案选择

采用方案 A：
- `C++20 + Qt 6 Widgets`
- **完整保留 WitchyBND 依赖，不做二进制与目录内容改动**
- WitchyBND 继续作为 sidecar bundled 依赖
- 原生应用负责：稳定调用、隔离工作区、manifest gate、路径策略、GUI 产品化与发布打包

补充定义：
- **不改动**：WitchyBND 二进制、目录内容、发布形态
- **必须可改动**：adapter、staging、调用策略、超时策略、输出识别、资格认证与验证流程

### 2.2 当前明确拒绝项

以下内容在当前 MVP 中明确不做：
- rollback 功能
- 历史与诊断页面
- AC mutation / AC experimental import
- Team 命令执行流

### 2.3 执行方式

执行流程改为：
- 通过 `$ralph` 作为主执行入口
- 由 Ralph 在执行中按任务切分启用对应数量的 subagent
- 待 subagent 完成后，由 Ralph 主 agent 统一整合、验证、收尾
- 不再使用 `$team`

### 2.4 固定 lane ownership

- Subagent-1 / Foundation-Build：CMake / Qt 6 / Windows x64 Release 构建与打包
- Subagent-2 / Sidecar-Workspace：WitchyBND adapter、manifest gate、staging workspace、路径/副产物隔离
- Subagent-3 / Emblem-Core：Emblem real-save parse、catalog、preview、share->user import 核心
- Subagent-4 / Product-GUI：普通用户 GUI：首页、徽章库、导入向导、保存结果页
- Subagent-5 / Verification-Evidence：测试、golden corpus、qualification suite、验收证据
- Ralph 主 agent：接口定稿、subagent 启停编排、跨 lane 整合、集成回归、文档更新、最终验收判定

执行约束：
- subagent 之间禁止共享写集
- 公共接口由 Ralph 主 agent 先定稿后再分派

### 2.5 每个 subagent 的明确 scope

| subagent | 负责对象 | 允许写入范围 | 必交付物 | 明确不负责 |
|---|---|---|---|---|
| Subagent-1 / Foundation-Build | 原生工程骨架、Windows x64 Release 构建、portable 目录契约 | `native/CMakeLists.txt`、`native/cmake/**`、`native/resources/version/**`、`scripts/build_native_windows.*`、`scripts/package_native_windows.*` | 可构建的 Qt6/CMake 工程、Release 构建脚本、portable 目录布局、包体清单 | WitchyBND 调用逻辑、emblem 解析、业务 UI 页面 |
| Subagent-2 / Sidecar-Workspace | WitchyBND 调用层、manifest gate、no-PATH-fallback、ASCII-safe staging 根目录、源目录零副产物 | `native/core/tool_adapter/**`、`native/core/workspace/**`、`native/platform/windows/process/**`、`native/config/sidecar/**` | Unicode-safe 调用、staging workspace、qualification suite 接口、失败证据采样 | `third_party/WitchyBND/**` 内容修改、GUI 页面、emblem 业务规则 |
| Subagent-3 / Emblem-Core | `USER_DATA007` parse/serialize、catalog、preview model、单徽章导入空槽位规则 | `native/core/emblem/**`、`native/tests/fixtures/emblem/**` | catalog DTO、preview DTO、single-share->empty-user-slot import plan/apply、readback 可重解析契约 | 外部工具启动、文件对话框、portable 打包 |
| Subagent-4 / Product-GUI | 首页、徽章库、导入向导、保存结果页、普通用户文案与流程 | `native/app/MainWindow.*`、`native/app/pages/**`、`native/app/presenters/**`、`native/app/viewmodels/**`、`native/resources/ui/**` | 普通用户 GUI 主流程、错误弹窗文案、输出路径选择、结果页 | 编解码核心、WitchyBND 调用、golden corpus |
| Subagent-5 / Verification-Evidence | 单元/集成/UI 测试、golden corpus、qualification suite、验收证据索引 | `native/tests/**`、`docs/appendices/**` 中验收证据索引文件、`artifacts/acceptance/**` 产物生成脚本 | test harness、sidecar qualification suite、验收记录模板、evidence manifest | 生产业务逻辑主实现、GUI 页面布局、third_party 内容 |
| Ralph 主 agent | 公共接口冻结、subagent 顺序编排、跨域整合、最终验收与文档收口 | 跨文件 glue code、接口声明、最终文档更新 | 接口冻结版本、集成结果、最终 verdict、后续阶段入口 | 不应长期接管任一 subagent 的大块 lane 内实现 |

### 2.6 Subagent 顺序与依赖

1. Ralph 主 agent 先冻结公共接口、DTO、错误码和证据目录契约，尤其先冻结两条最高冲突边界：
   - Sidecar-Workspace <-> Emblem-Core：打开存档 DTO、导入输入/输出 DTO、readback 契约
   - Emblem-Core <-> Product-GUI：catalog DTO、preview DTO、错误状态与结果页映射
2. Subagent-1 与 Subagent-2 可并行启动：
   - Subagent-1 先搭建工程与打包骨架
   - Subagent-2 先打通 WitchyBND sidecar + staging
3. Subagent-3 在接口冻结后启动，早期可先对接 fixture / golden corpus，再切真实 sidecar read model。
4. Subagent-4 在 DTO 契约冻结后启动，允许先对接 mock provider，但合并前必须切真实 stable-emblem provider。
5. Subagent-5 从一开始建立测试与证据骨架，待 1-4 产出后执行 qualification、集成回归与验收证据收集。
6. 所有 subagent 返回后，由 Ralph 主 agent 统一合并、修补契约差异、完成最终回归与文档更新。

## 3. ADR

### Decision
采用 `C++20 + Qt 6 Widgets + WitchyBND sidecar` 作为长期主线；普通用户主流程重构为“打开存档 -> 浏览/预览 -> 导入分享徽章 -> 另存为新存档”，把 dry-run / apply / readback / fail-closed 下沉为内部机制。

### Drivers
1. 长期维护与正式发布。
2. 普通用户可理解、可测试、可验收。
3. 在保留 WitchyBND 不变的前提下尽量缩小包体。
4. 避免当前 Python 产品线继续演化为正式主线。

### Alternatives considered
1. `C++ + Win32/WTL + WitchyBND`：可能更轻，但 UI 迭代成本高。
2. `C++ + Qt + 立即替换 WitchyBND`：长期更优，但当前风险过高。
3. 继续 Python：已被用户驳回，不再作为主线。

### Why chosen
- Qt Widgets 在 Windows 桌面产品里兼顾开发效率、可维护性、界面控制、长期扩展能力。
- 保留 WitchyBND 不变，可以避免本阶段把容器工具链替换与产品重构耦合在一起。
- 原生重写可避免当前 Python/PyInstaller 路线的运行时和交付膨胀。

### Consequences
- 短期需要维护两条线：
  - Python 参考/验证线
  - C++ 正式交付线
- 首阶段包体不会极小，因为 WitchyBND sidecar 本身体积较大。
- 打开存档的工作方式必须改为“内部 staging workspace”，不能继续让 sidecar 直接作用于用户原始目录。

### Follow-ups
- N2 评估进一步压缩包体，但在当前决策下不修改 WitchyBND 本身。
- Python 线冻结，仅保留语义参考与逆向验证价值。

## 4. 产品原则

1. 主界面必须使用普通用户语义，不暴露内部工程术语。
2. 所有写入安全机制继续保留，但默认隐藏在产品流程后面。
3. 用户原始存档只读；任何中间处理都必须在内部 staging workspace 进行。
4. 输出必须是新的存档文件，不允许覆盖原始存档。
5. 当前 MVP 只覆盖 Emblem stable lane，不承诺 AC 写入。

## 5. 用户与主流程

## 5.1 目标用户
- ARMORED CORE VI 普通玩家
- 需要浏览、导入分享徽章、保存到新存档
- 不关心 dry-run、workspace、audit、rollback 等工程概念

## 5.2 普通用户主流程
1. 启动程序
2. 打开真实 `.sl2` 存档
3. 浏览用户徽章与分享徽章
4. 查看预览图和目标槽位
5. 选择分享徽章并执行导入
6. 保存为新存档

## 5.3 页面结构

### 首页
- 打开存档
- 最近存档
- 关于当前支持范围（仅 Emblem stable lane）

### 徽章库页
- 用户徽章 / 分享徽章分区
- 搜索与筛选
- 预览与详情
- 导入按钮

### 导入向导
- 选定分享徽章
- 自动推荐可用槽位
- 明确提示：将保存为新存档
- 最终动作：`开始导入`

### 保存结果页
- 显示新存档输出路径
- 显示成功/失败结果
- 提供“在资源管理器中打开输出目录”
- 失败时提供“复制错误详情”或“打开支持包目录”

## 6. MVP 范围

## 6.1 In Scope
- Windows x64 桌面应用
- C++20 + Qt Widgets 工程
- 完整保留 WitchyBND sidecar
- `third_party/WitchyBND/**` 目录级 manifest gate
- no-PATH-fallback
- Unicode-safe 外部工具调用
- ASCII-safe 短 staging 根目录
- 内部 staging workspace
- 打开真实 `.sl2` 存档
- 浏览 user/share emblem library
- 徽章预览
- 选定分享徽章导入到使用者槽位
- 保存为新存档
- 必要的错误提示与失败阻断

## 6.2 Out of Scope
- rollback
- 历史与诊断页面
- AC import / AC experimental import
- 安装器、自动更新
- 跨平台
- WitchyBND 替换或定制化修改

## 7. 不变量

即使 rollback 与历史页面暂时砍掉，以下内部安全契约仍必须保留：
- shadow workspace
- post-write readback
- no in-place overwrite
- fresh-destination-only
- fail-closed
- 用户原始存档只读

注：`dry-run` 不再作为主 UI 主按钮语义，但它可以作为内部导入前检查机制存在。

### 7.1 当前 MVP 额外限制

- 当前 N1 只支持**单个分享徽章**导入
- 当前 N1 只支持导入到**空闲 user 槽位**
- 当前 N1 不支持批量导入
- 当前 N1 不支持槽位替换或挤占
- 当前 N1 不提供 rollback 页面；若用户对结果不满意，删除新存档并从原始存档重新导入

## 8. 非功能要求

### 8.1 兼容性
- 操作系统：Windows 10 x64（重点验收环境：Windows 10 25H2）
- 机器：`ZPX-GE77`
- 杀软环境：Huorong 开启

### 8.2 包体目标
- 由于 WitchyBND sidecar 完整保留不变，包体目标按阶段设定：
  - N1 首版目标：portable zip 尽量控制在 `120MB-150MB` 区间
  - 若超过该目标，必须给出按文件粒度的体积说明

### 8.3 可维护性
- CMake 工程可独立构建
- UI、workspace、adapter、emblem core 明确分层
- 关键流程必须有自动化测试与真机验收清单

## 9. 里程碑

## N0：Native Foundation
目标：证明原生程序能在不污染源目录的前提下，稳定驱动 WitchyBND 打开真实 `.sl2`。

范围：
- CMake + Qt Widgets 工程
- Windows x64 Release 构建
- WitchyBND sidecar 适配器
- `third_party/WitchyBND/**` manifest gate
- no-PATH-fallback
- staging workspace
- ASCII-safe 短 staging 根目录
- Unicode-safe 路径处理
- sidecar qualification suite
- 打开真实 `.sl2` 最小 smoke

验收：
- 目标样本集中的真实 `.sl2` 打开成功率 100%
- 源目录零副产物
- sidecar 在中文路径 / OneDrive 路径下稳定工作
- 任一 sidecar 失败都能采集 return code / stdout / stderr 摘要

## N1：User-first Emblem MVP
目标：普通用户可完成一次完整徽章导入与另存为流程。

范围：
- 首页
- 徽章库页
- 导入向导
- 保存结果页
- real-save emblem catalog / preview / share->user import
- save-as-new
- 单徽章、单槽位、单结果页 MVP

验收：
- 普通用户不需要理解 `dry-run / apply / rollback`
- 能从真实 `.sl2` 导入一个分享徽章并另存为新存档
- 游戏内可见、可编辑、可保存

## N1.5：Native Packaging & Live Acceptance
目标：形成可发布的 Windows x64 portable 交付与现场验收流程。

范围：
- portable release
- ZPX-GE77 真机验收
- Huorong 场景验证
- 游戏内人工复核

验收：
- 首启成功
- 打开存档成功
- 导入并另存为成功
- 二次启动成功
- 游戏内验证通过

## 10. 执行建议（仅 Ralph）

### available-agent-types roster
- architect
- executor
- designer
- build-fixer
- debugger
- test-engineer
- verifier
- writer
- critic
- code-reviewer

### 建议的 `$ralph` 执行命令

```text
$ralph "按 docs/ac6-data-manager.native-cpp.prd.md、docs/ac6-data-manager.native-cpp.technical-design.md 与 docs/appendices/ac6-data-manager.native-cpp.acceptance-baseline.md 执行 N0、N1。执行方式：Ralph 先冻结接口、错误码、DTO、证据目录和写集边界，再启用 5 个 subagent。Subagent-1 负责 CMake/Qt6 工程骨架、Windows x64 Release 构建与 portable 打包；Subagent-2 负责 WitchyBND manifest gate、Unicode-safe adapter、ASCII-safe staging workspace、源目录零副产物约束；Subagent-3 负责 Emblem real-save parse、catalog、preview、单徽章 share->user import；Subagent-4 负责普通用户 GUI：首页、打开存档、导入向导、保存为新存档；Subagent-5 负责测试、qualification suite、golden corpus 与验收 evidence manifest。待 subagent 完成后由 Ralph 主 agent 统一集成、回归、更新文档并给出 verdict。验收：真实 .sl2 打开稳定、源目录零副产物、普通用户主流程闭环、Windows x64 构建通过。继续保持 shadow workspace、post-write readback、no in-place overwrite、fail-closed；不暴露 rollback 与历史/诊断页面。"
```
