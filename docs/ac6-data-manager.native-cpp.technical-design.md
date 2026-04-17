# AC6 Data Manager Native C++ Technical Design

- 文档状态：Approved for Execution
- 版本：v1.0-native-cpp
- 最后更新：2026-04-14
- 关联 PRD：`docs/ac6-data-manager.native-cpp.prd.md`
- 关联验收基线：`docs/appendices/ac6-data-manager.native-cpp.acceptance-baseline.md`

## 1. 设计目标

在完整保留 WitchyBND 依赖不变的前提下，使用 `C++20 + Qt 6 Widgets` 实现面向普通用户的 Windows x64 桌面产品，并把当前 Python 参考线中的 Emblem stable lane 能力迁移到原生应用。

本轮只覆盖：
- N0：Native Foundation
- N1：User-first Emblem MVP

本轮明确不做：
- rollback
- 历史与诊断页面
- AC lane
- sidecar 替换

## 2. 总体架构

```text
native/
  app/
    main.cpp
    AppController.*
    MainWindow.*
    pages/
      HomePage.*
      EmblemLibraryPage.*
      ImportWizard.*
      SaveResultPage.*
  core/
    tool_adapter/
      WitchyBndProcessAdapter.*
    workspace/
      SessionWorkspace.*
      SaveStagingService.*
      ReadbackVerifier.*
    emblem/
      UserData007Codec.*
      EmblemCatalogService.*
      EmblemPreviewService.*
      ShareImportPlanner.*
      EmblemApplyService.*
    audit/
      OperationOutcome.*
      EvidenceWriter.*
  platform/windows/
    FileDialogService.*
    ShellLaunchService.*
  tests/
    unit/
    integration/
    ui/
```

## 3. 技术栈与依赖

### 3.1 栈
- 语言：`C++20`
- GUI：`Qt 6 Widgets`
- 构建：`CMake`
- 编译器：`MSVC x64`
- 日志：`spdlog`
- JSON：`nlohmann/json`
- 测试：`GoogleTest` + `Qt Test`

### 3.2 外部工具依赖
- `third_party/WitchyBND/**` 完整保留，不做二进制或目录结构修改
- 依赖策略：bundled sidecar
- 调用方式：仅通过原生 adapter 层调用，不允许业务层直接拼命令
- N0 起继承目录级 manifest gate 与 no-PATH-fallback

## 4. 为什么选 Qt Widgets

1. 适合 Windows 桌面工具长期维护。
2. 控件体系成熟，适合实现“首页 / 库页 / 向导页 / 结果页”这类传统桌面产品。
3. 对 Unicode 路径、QProcess、文件对话框、资源管理器交互支持完整。
4. 相比 Win32/WTL，能更快完成产品化 GUI，同时仍保持原生 C++ 路线。

## 5. 关键设计决策

## 5.1 WitchyBND 保留但必须被隔离

WitchyBND 不改动，但不能直接作用于用户选中的源目录。

改造要求：
1. 用户选中的 `source_save` 首先复制到内部 staging workspace。
2. WitchyBND 只能处理 staging copy。
3. 解包产生的中间目录、副产物、`.bak/.backup` 等都只能留在 staging workspace。
4. 用户源目录必须零副产物。

补充定义：
- 不改动 WitchyBND，不代表不改动产品侧调用契约
- adapter、staging、输出定位、超时和资格认证必须作为产品侧职责实现

### 5.1.1 Sidecar Qualification Suite（N0 硬门槛）

N0 必须建立 sidecar qualification suite，覆盖：
- 中文路径
- OneDrive 路径
- 长路径
- staging 内 unpack / repack
- 副产物只落在 staging
- return code / stdout / stderr 可采样
- 输出定位规则稳定

只有 sidecar qualification suite 通过，N1 才允许投入 GUI 完整流程。

## 5.2 内部安全机制保留，但不暴露给普通用户

以下机制继续保留：
- 导入前检查（内部 dry-run）
- shadow workspace
- post-write readback
- fresh-destination-only
- fail-closed

但在 UI 文案中，不再以技术按钮形式暴露：
- 不出现 `dry-run`
- 不出现 `apply`
- 不出现 `rollback`
- 不出现“审计视图”主导航

补充要求：
- 当前 MVP 不提供 rollback 与历史/诊断页面
- 但失败时仍必须保留最小支持出口：复制错误详情、打开支持包目录或导出故障包
- 最小支持出口只允许在“打开存档失败”或“保存为新存档失败”时显示；成功路径不得出现“支持包”“诊断”“审计”主入口
- 最小支持出口文案固定为：`复制错误详情`、`打开支持包目录`

## 5.3 用户语义映射

| 内部机制 | 用户可见语义 |
|---|---|
| source_save | 打开的存档 |
| dry-run | 导入前检查 |
| apply | 保存为新存档 |
| output_save | 新存档位置 |
| readback | 自动校验 |
| rollback | 本阶段不提供用户功能 |
| audit | 本阶段不提供单独页面 |

## 6. UI 信息架构

## 6.1 首页
控件：
- `打开存档`
- `最近打开`
- `仅支持徽章导入` 说明

状态：
- 未打开存档
- 打开成功
- 打开失败

## 6.2 徽章库页
区域：
- 左侧：分类（使用者 / 分享）
- 顶部：搜索、筛选
- 中部：徽章列表/缩略图
- 右侧：详情、预览、推荐目标槽位
- 底部：`导入所选徽章`

## 6.3 导入向导
步骤：
1. 确认已选分享徽章
2. 自动推荐目标槽位
3. 说明将保存为新存档
4. 选择输出路径
5. 开始导入

导入向导内部动作：
- 执行导入前检查
- 如检查失败则阻断并给出用户友好错误
- 如检查通过则执行写入与自动校验

## 6.4 保存结果页
展示：
- 导入是否成功
- 新存档输出路径
- 可打开输出目录
- 可复制错误详情 / 打开支持包目录（失败时）
- 可回到徽章库继续操作

失败文案约束：
- 打开存档失败：`无法打开该存档，请确认文件未被占用。`
- 保存失败：`保存失败，未生成新存档。`

## 7. 核心流程设计

## 7.1 打开存档

```text
用户选择 source_save
-> AppController 校验文件存在且为 .sl2
-> 通过 bundled sidecar manifest gate 与 no-PATH-fallback 检查
-> SessionWorkspace 创建 staging 根目录
-> 复制 source_save 到 staging/input/
-> WitchyBndProcessAdapter 对 staging copy 执行 unpack
-> 读取 staging/unpacked/USER_DATA007
-> UserData007Codec 解析 emblem 数据
-> EmblemCatalogService 生成 user/share catalog
-> MainWindow 刷新为 real-save 视图
```

### 打开存档通过条件
- 原始目录不产生任何副产物
- 中文路径/OneDrive 路径可稳定工作
- 失败时不会改动源文件
- 任一失败都能采集 sidecar return code / stdout / stderr 摘要

## 7.2 导入并另存为新存档

```text
用户在徽章库页选定分享徽章
-> 进入导入向导
-> ShareImportPlanner 计算目标槽位
-> 内部执行导入前检查
-> EmblemApplyService 修改 staging 中的 USER_DATA007
-> WitchyBndProcessAdapter 执行 repack
-> ReadbackVerifier 自动校验写后结果
-> 仅当校验通过，输出到用户选择的 fresh destination
-> SaveResultPage 展示成功
```

当前 MVP 限制：
- 只支持单个分享徽章导入
- 只支持空闲 user 槽位
- 槽位已满时直接阻断，不做替换、不做批量

### 失败处理
- 任何一步失败都 fail-closed
- 不输出新存档
- 不修改原始存档
- 错误以普通用户文案呈现，例如：
  - `无法打开该存档，请确认文件未被游戏占用`
  - `导入失败，未生成新存档`

## 8. 模块说明

## 8.1 WitchyBndProcessAdapter
职责：
- 统一封装 sidecar 调用
- 使用 `CreateProcessW` 或 `QProcess`
- 捕获 return code、stdout、stderr
- 处理超时
- 保证只在 staging 路径中运行

约束：
- 不允许直接接受用户原始路径作为工作目录
- 不允许在 adapter 层暴露临时副产物给 UI
- staging 根目录固定为本地、短路径、ASCII-safe 的应用私有目录，例如 `%LOCALAPPDATA%/AC6DM/runtime/<session-id>/`

## 8.2 SessionWorkspace / SaveStagingService
职责：
- 为每次“打开存档”创建独立 staging 会话
- 管理 `input/`、`unpacked/`、`repacked/`、`temp/`
- 清理旧会话

## 8.3 UserData007Codec
职责：
- 解析和序列化 `USER_DATA007`
- 仅覆盖 Emblem stable lane 所需字段
- 保持与 Python 参考线语义一致

## 8.4 EmblemCatalogService
职责：
- 从 `USER_DATA007` 生成 user/share catalog
- 为 UI 提供列表、来源、可编辑性、目标槽位信息

## 8.5 ShareImportPlanner
职责：
- 选择可用 user 槽位
- 处理 share -> user 的映射计划
- 生成用户可读的导入摘要

## 8.6 ReadbackVerifier
职责：
- 对 repack 后结果做自动校验
- 验证关键对象是否可重新解析
- 校验失败则阻断输出

## 8.7 SupportEvidenceService
职责：
- 记录 operation id、source hash、output path、sidecar exit code、stderr 摘要、readback verdict
- 失败时生成最小支持包
- 供 UI 的失败页或提示框调用，不单独暴露历史/诊断页面

## 9. 打包设计

## 9.1 交付形态
- Windows x64 portable 目录
- 主程序 exe
- Qt 运行时 DLL
- `third_party/WitchyBND/`
- 说明文档

## 9.2 包体控制策略
在不改动 WitchyBND 前提下，体积控制手段包括：
1. 仅链接必要 Qt 模块：`Core / Gui / Widgets`
2. 避免引入 QML、WebEngine、多媒体等非必要模块
3. Release 编译、关闭调试符号随包发布
4. 明确列出产物体积清单

## 10. 测试设计

## 10.1 单元测试
- `UserData007Codec`
- `EmblemCatalogService`
- `ShareImportPlanner`
- `ReadbackVerifier`
- `WitchyBndProcessAdapter` 参数组装与错误处理

## 10.2 集成测试
- 打开真实 `.sl2`
- 中文路径 / OneDrive 路径
- share -> user 导入
- 单徽章、空闲槽位约束
- fresh-destination-only
- 源目录零副产物
- sidecar manifest gate / no-PATH-fallback

## 10.3 UI Smoke
- 首页 -> 打开存档 -> 徽章库 -> 导入向导 -> 保存结果

## 10.4 人工验收
- ZPX-GE77
- Windows 10 25H2
- Huorong enabled
- 游戏内验证：可见 / 可编辑 / 可保存

## 11. Ralph + subagent 执行模式

本项目后续不使用 `$team`，统一采用 Ralph 主执行流：
- Ralph 主 agent 负责阶段调度、验证、文档收口
- 默认采用 5 个 subagent：
  - Subagent-1：Foundation-Build
  - Subagent-2：Sidecar-Workspace
  - Subagent-3：Emblem-Core
  - Subagent-4：Product-GUI
  - Subagent-5：Verification-Evidence
- 所有 subagent 返回后，由 Ralph 主 agent 做：
  - 契约统一
  - 集成回归
  - 文档更新
  - 交付物确认

执行约束：
- subagent 之间禁止共享写集
- 主 agent 先定公共接口，再分派 lane

### 11.1 默认 subagent scope matrix

#### Subagent-1：Foundation-Build
- 目标：把 `native/` 原生工程编译、链接、打包链路建立起来。
- 允许写入：
  - `native/CMakeLists.txt`
  - `native/cmake/**`
  - `native/resources/version/**`
  - `scripts/build_native_windows.*`
  - `scripts/package_native_windows.*`
- 输入：
  - Ralph 冻结后的目录契约
  - Qt6 / MSVC / Windows x64 构建要求
- 输出：
  - Release 可构建
  - portable 目录结构
  - 包体清单与构建说明
- 非职责范围：
  - 不实现 WitchyBND 调用
  - 不实现 emblem 编解码
  - 不改业务 GUI 页面逻辑

#### Subagent-2：Sidecar-Workspace
- 目标：把 WitchyBND 调用、manifest gate、staging workspace 做成稳定底座。
- 允许写入：
  - `native/core/tool_adapter/**`
  - `native/core/workspace/**`
  - `native/platform/windows/process/**`
  - `native/config/sidecar/**`
- 输入：
  - bundled `third_party/WitchyBND/**`
  - manifest gate / no-PATH-fallback / ASCII-safe staging 约束
- 输出：
  - Unicode-safe process adapter
  - ASCII-safe 本地短 staging 根目录
  - 源目录零副产物保证
  - stdout/stderr/return code 采样
- 非职责范围：
  - 不修改 `third_party/WitchyBND/**`
  - 不做 emblem 业务规则
  - 不做 GUI 页面

#### Subagent-3：Emblem-Core
- 目标：实现 `USER_DATA007` 的稳定徽章能力主链。
- 允许写入：
  - `native/core/emblem/**`
  - `native/tests/fixtures/emblem/**`
- 输入：
  - Ralph 冻结后的 DTO / service interface
  - real-save / fixture 样本
- 输出：
  - parse / serialize
  - user/share catalog
  - preview model
  - single-share -> empty-user-slot import plan / apply
  - readback 重解析可通过
- 非职责范围：
  - 不负责文件对话框
  - 不负责外部进程调用
  - 不负责打包链路

#### Subagent-4：Product-GUI
- 目标：把内部安全流程包装成普通用户可测试的桌面主流程。
- 允许写入：
  - `native/app/MainWindow.*`
  - `native/app/pages/**`
  - `native/app/presenters/**`
  - `native/app/viewmodels/**`
  - `native/resources/ui/**`
- 输入：
  - Ralph 冻结后的 DTO / error model
  - Emblem-Core 提供的 catalog / preview / import service
- 输出：
  - 首页
  - 徽章库页
  - 导入向导
  - 保存结果页
  - 固定用户文案与失败路径入口
- 非职责范围：
  - 不改编解码核心
  - 不改 WitchyBND adapter
  - 不维护 golden corpus

#### Subagent-5：Verification-Evidence
- 目标：把“可执行”变成“可验证、可验收、可追溯”。
- 允许写入：
  - `native/tests/**`
  - `docs/appendices/**` 中验收证据索引文件
  - `artifacts/acceptance/**` 生成脚本
- 输入：
  - 验收基线文档
  - real-save 样本与 golden corpus
- 输出：
  - unit / integration / UI harness
  - sidecar qualification suite
  - evidence manifest
  - live acceptance checklist
- 非职责范围：
  - 不拥有生产代码主逻辑
  - 不负责 GUI 交互设计
  - 不修改 third_party sidecar 内容

### 11.2 Ralph 主 agent scope

- 负责：
  - 冻结公共接口、DTO、错误码、证据目录约定
  - 决定 subagent 启动顺序与依赖
  - 处理跨 lane glue code
  - 完成最终集成回归、文档收口、阶段 verdict
- 不负责：
  - 长期替代某一 subagent 完成大块 lane 内实现
  - 在接口未冻结前放任多个 subagent 修改同一契约

高冲突接口，必须在 Ralph 启动 Subagent-3 / Subagent-4 前冻结：
- Sidecar-Workspace <-> Emblem-Core：
  - open-save result DTO
  - import request / import result DTO
  - readback verification result contract
- Emblem-Core <-> Product-GUI：
  - catalog / preview DTO
  - slot recommendation DTO
  - error state / result-page status mapping

### 11.3 推荐执行顺序

1. Ralph：先写接口冻结与写集边界。
2. 并行启动：
   - Subagent-1 Foundation-Build
   - Subagent-2 Sidecar-Workspace
3. 接口冻结后启动：
   - Subagent-3 Emblem-Core
   - Subagent-4 Product-GUI
4. 从项目开始即铺设、集成前集中收口：
   - Subagent-5 Verification-Evidence
5. 所有 subagent 完成后，由 Ralph 执行：
   - 契约对齐
   - 集成构建
   - 回归测试
   - 文档和验收证据更新
