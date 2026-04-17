# AC6 Data Manager Native C++ N2 PRD：Asset Library & Exchange

- 文档状态：Approved / N2A Implemented / N2B-N2D In Progress
- 版本：v2.0-native-cpp-n2
- 最后更新：2026-04-15（N2B/N2C/N2D docs-evidence lane 收口）
- 技术路线：`C++20 + Qt 6 Widgets + WitchyBND sidecar`
- 关联技术方案：`docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`
- 关联验收基线：`docs/appendices/ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`
- 继承基线：
  - `docs/ac6-data-manager.native-cpp.prd.md`
  - `docs/ac6-data-manager.native-cpp.technical-design.md`
  - `docs/appendices/ac6-data-manager.native-cpp.acceptance-baseline.md`

## 1. 目标

在当前 Native C++ 版已经能够：
- 稳定打开真实 `.sl2` 存档
- 在真实存档中完成单个分享徽章导入闭环
- 把运行时工作区隔离在程序目录内
- 通过 WitchyBND sidecar 完成 unpack / repack

的基础上，进入 `N2 — Asset Library & Exchange` 阶段，逐步把产品从“单一徽章导入工具”扩展为“面向普通玩家的 AC / 徽章资产管理器”。

N2 的总目标分为四段：
1. 先把 GUI 与数据模型升级为统一资产目录壳，覆盖 `徽章 + AC` 双域浏览。
2. 先完成 **徽章 stable 产品化**，包括指定使用者栏位导入与文件交换格式。
3. 在 AC 仍保持只读的前提下，完成 record-map、preview provenance 与资格认证。
4. 最后才开放 AC 文件交换与指定使用者栏位导入。

## 1.1 当前实施状态

### 已完成并固化到代码/验证的范围
- `N2A` 已进入 implemented 状态。
- 已有 fresh release 目录与 probe 证据证明：
  - 可稳定打开真实 `.sl2`；
  - `徽章库 / AC库` 双视图存在；
  - 徽章与 AC 已按 `使用者1/2/3/4 + 分享` 五类来源建模；
  - `将所选徽章导入使用者栏` 已走显式目标栏位弹窗；
  - AC 仍保持 read-only，相关动作 disabled 并保留 reason。
- N2A 自动化回归、UI 测试与 probe 证据已记录到：
  - `docs/appendices/ac6-data-manager.native-cpp.n2a-gui-evidence-index.md`

### 当前进入执行的范围
- `N2B`：徽章文件交换 `.ac6emblemdata` v1 与 GUI 收口。
- `N2C`：AC gate 资格认证与 qualification 证据闭环。
- `N2D`：`.ac6acdata` 与 AC 双域最终 GUI 收口。

### 当前仍未开放给普通用户的能力
- `将所选AC导入使用者栏`
- `导入AC文件到使用者栏`
- `导出所选AC`

以上三项必须等 `N2C` gate 全部通过后，才允许在 `N2D` 打开。

## 2. 本轮已确认决策

### 2.1 阶段结构采用 N2A / N2B / N2C / N2D

本阶段不采用旧的线性 M1/M2/M3/M4 对称拆分，而采用经 consensus 审核通过的四段结构：

- `N2A — 统一资产目录壳 + 徽章稳定产品化 + AC 只读探测`
- `N2B — 徽章交换格式 v1 + GUI 收口`
- `N2C — AC 资格认证（record-map / preview / write gate）`
- `N2D — AC 交换格式 v1 + 最终双域 GUI 收口`

### 2.2 语言与外部工具决策不变

继续采用：
- `C/C++` 原生路线
- `Qt Widgets` GUI
- `WitchyBND sidecar` 完整保留，不修改其二进制与目录内容

继续禁止：
- 修改 WitchyBND 本体
- 原地覆盖源存档
- 在未资格化前开放 AC mutation

### 2.3 普通用户产品原则继续加强

从 N2 开始，每个 MVP 都必须满足：
- GUI 覆盖该阶段承诺的全部可测范围
- 普通用户能通过按钮、弹窗、提示完成测试，不依赖命令行
- 开发者术语继续下沉，不作为正常用户主流程入口

补充规定：
- `dry-run / apply / rollback / audit` 不再作为主界面按钮语义
- GUI 必须直接提供用户理解得了的动作词，例如：
  - `打开存档`
  - `将所选徽章导入使用者栏`
  - `将所选AC导入使用者栏`
  - `导入徽章文件到使用者栏`
  - `导入AC文件到使用者栏`
  - `导出所选徽章`
  - `导出所选AC`

### 2.4 交换格式命名决策

N2 阶段统一采用独立文件格式：
- 徽章交换格式：`.ac6emblemdata`
- AC 交换格式：`.ac6acdata`

已明确废弃：
- `.ac6preset`

### 2.5 当前验收输入物已满足最低要求

用户已经确认：
- 真实人工验收用测试存档已提供
- Windows 验收环境信息已提供
- WitchyBND 可随程序一起提供，且保持与 ACVIEmblemCreator 一致的使用方式
- 首版可只支持 Windows x64
- 当前交付以未压缩 release 目录为主，不再要求额外 zip 打包

## 3. 需求总表

### 3.1 资产分类

打开任意 `.sl2` 后，程序必须把 **徽章** 与 **AC** 都按以下五类来源建模：
- `使用者1`
- `使用者2`
- `使用者3`
- `使用者4`
- `分享`

该五类必须同时体现在：
- 数据模型
- GUI 分类视图
- 详情面板
- 导入目标栏位选择弹窗
- 后续文件导入导出

### 3.2 预览

必须支持：
- `AC`：优先使用存档中的原生预览图片；若链路未稳定，必须显示 provenance 状态
- `徽章`：优先读取原生预览；若不存在，再使用可验证的衍生渲染；若仍不可得，明确显示无预览

### 3.3 指定使用者栏位导入

必须支持普通用户语义下的两类“存档内复制”：
1. `将所选徽章导入使用者栏`
2. `将所选AC导入使用者栏`

点击后必须出现目标栏位选择弹窗，由用户明确选择：
- 使用者1
- 使用者2
- 使用者3
- 使用者4

不得继续使用“自动找第一个空槽位”作为唯一用户语义；自动推荐可以保留，但必须允许用户选择目标栏位。

### 3.4 文件导入导出

必须最终支持两类文件交换闭环：

#### 徽章
- `导出所选徽章` -> 导出为 `.ac6emblemdata`
- `导入徽章文件到使用者栏` -> 从 `.ac6emblemdata` 导回任意已打开存档，并让用户选择目标使用者栏位

#### AC
- `导出所选AC` -> 导出为 `.ac6acdata`
- `导入AC文件到使用者栏` -> 从 `.ac6acdata` 导回任意已打开存档，并让用户选择目标使用者栏位

## 4. N2 分阶段范围

## 4.1 N2A：统一资产目录壳 + 徽章稳定产品化 + AC 只读探测

### 目标
把当前仅支持徽章的 MVP 升级为双域浏览壳，并完成“指定使用者栏位的徽章导入”。同时把 AC 拉入只读目录浏览与 preview provenance 调查，但 **绝不开放 AC 写入**。

### In Scope
- 统一资产目录壳：`徽章库 / AC库` 双视图
- 徽章五类来源建模：使用者1/2/3/4 + 分享
- AC 五类来源建模：使用者1/2/3/4 + 分享
- 徽章详情与预览
- AC 详情与预览状态
- 徽章：`将所选徽章导入使用者栏`
- 徽章目标栏位选择弹窗
- AC：只读 record-map、只读详情、只读预览状态
- GUI 中未开放能力必须 disabled 并说明原因

### Out of Scope
- 徽章文件导入导出
- AC 文件导入导出
- AC 写入
- rollback / 历史 / 诊断页面

### N2A GUI 覆盖要求
本阶段 GUI 必须至少覆盖：
1. 打开真实存档
2. 切换 `徽章库 / AC库`
3. 浏览五类来源
4. 选择徽章并查看详情/预览
5. 点击 `将所选徽章导入使用者栏`
6. 弹出目标使用者栏位选择框
7. 完成写入并回读验证
8. 在 AC 视图看到五类来源、详情与 preview provenance 状态
9. 在 AC 未开放按钮上看到明确禁用原因

## 4.2 N2B：徽章交换格式 v1 + GUI 收口

### 当前执行要求
- 文件扩展名固定为 `.ac6emblemdata`。
- 仅允许在现有 stable-emblem lane 上增量实现，不得回退 N2A 已验证的目标栏位导入流程。
- GUI 必须把“存档内复制”和“外部交换文件导入”明确区分为两组动作。
- N2B 完成后，普通用户必须能在 GUI 中完成：
  - 选择一个已打开存档中的徽章并导出为 `.ac6emblemdata`；
  - 打开另一个存档；
  - 选择 `导入徽章文件到使用者栏`；
  - 在弹窗中指定 `使用者1/2/3/4`；
  - 输出 fresh destination 并重新打开验证。

### N2B 文档与证据收口位置
- `docs/appendices/ac6-data-manager.native-cpp.n2b-qualification-and-evidence.md`


### 目标
完成徽章外部文件交换闭环，并把 GUI 收口到普通用户可直接使用的稳定版本。

### In Scope
- `.ac6emblemdata` v1
- `导出所选徽章`
- `导入徽章文件到使用者栏`
- 目标使用者栏位选择弹窗
- GUI 收口：减少开发占位信息、统一文案与结果提示

### 阶段通过条件
- 徽章可从存档导出为独立文件
- 独立文件可导入任意打开的存档指定使用者栏位
- 文件格式具备版本号、来源摘要、最小兼容元数据

## 4.3 N2C：AC 资格认证

### 当前执行要求
N2C 的核心不是“尽快开放 AC”，而是把 AC 从只读探测提升到“已资格化、可审计”的稳定能力。

### N2C 输出物
- `Gate-AC-01` 记录模板与结论
- `Gate-AC-02` 记录模板与结论
- `Gate-AC-03` 记录模板与结论
- AC qualification summary
- gate 失败时的 blocker、样本范围、回滚策略与下一步动作

### N2C 文档与证据收口位置
- `docs/appendices/ac6-data-manager.native-cpp.n2c-ac-gate-record.md`


### 目标
在不急于开放 AC 写入的前提下，先把 AC 的技术风险闭合。

### 必须通过的三道 AC Gate
- `Gate-AC-01`：record-map stable
- `Gate-AC-02`：preview provenance stable
- `Gate-AC-03`：对指定使用者栏位写入并 readback 成功

### In Scope
- AC record-map 逆向与稳定化
- AC 预览链来源确认
- 受控写入实验与回读验证
- 资格认证报告

### 阶段约束
- 在 N2C 验证完成前，AC 仍不能进入 stable 用户写入主流程

## 4.4 N2D：AC 交换格式 v1 + 最终双域 GUI 收口

### 当前执行要求
- 仅当 `Gate-AC-01/02/03` 全部显式标记为 `PASS`，才允许进入 N2D 的用户可见 AC mutation。
- `.ac6acdata` 的用户故事必须与徽章文件交换对齐：
  - 从已打开存档导出所选 AC；
  - 从 `.ac6acdata` 导入到另一份已打开存档；
  - 用户显式选择目标使用者栏位；
  - 输出 fresh destination 并 readback。
- GUI 最终收口后，普通用户应能在一个稳定主界面中完成：
  - 徽章浏览/预览/导入导出
  - AC 浏览/预览/导入导出
  - disabled / locked action 的阶段原因说明

### N2D 文档与证据收口位置
- `docs/appendices/ac6-data-manager.native-cpp.n2d-qualification-and-evidence.md`


### 目标
在 AC gate 全部通过后，开放 AC 文件交换与指定使用者栏位导入，并完成最终双域 GUI 收口。

### In Scope
- `.ac6acdata` v1
- `将所选AC导入使用者栏`
- `导入AC文件到使用者栏`
- `导出所选AC`
- 双域 GUI 终态收口

## 5. 统一产品模型

N2 全阶段统一采用最小资产目录模型：
- `asset_kind`
- `source_bucket`
- `slot_index`
- `display_name`
- `preview_state`
- `write_capability`
- `record_ref`
- `detail_provenance`

其中：
- `asset_kind ∈ { emblem, ac }`
- `source_bucket ∈ { user1, user2, user3, user4, share }`
- `preview_state` 必须能表达：
  - `native_embedded`
  - `derived_render`
  - `unavailable`
  - `unknown`
- `write_capability` 至少能表达：
  - `editable`
  - `read_only`
  - `locked_pending_gate`

## 6. 产品交互原则

### 6.1 目标栏位选择必须显式化

用户点击“导入到使用者栏”后，必须明确选择目标使用者栏位。允许：
- 给出推荐栏位
- 高亮空闲栏位
- 警示将覆盖或阻断的栏位

但不允许：
- 不经选择直接写入
- 仅依赖后台自动选位

### 6.2 未开放能力必须以 disabled 形式展示

对本阶段尚未开放的功能，GUI 应显示禁用按钮与原因，而不是完全隐藏。例如：
- `将所选AC导入使用者栏`：`未开放：等待 AC 写入资格认证`
- `导出所选AC`：`未开放：等待 AC exchange v1`

### 6.3 GUI 必须覆盖测试范围

自本阶段起，任何 PRD 中被标记为当前 MVP In Scope 的功能，都必须在 GUI 中存在可点击、可触发、可观察结果的入口；不能只在命令行或探针中可测。

## 7. 约束与不变量

继续保持以下硬约束：
- `WitchyBND` 完整保留，不修改其本体
- `shadow / staging workspace`
- `fresh-destination-only`
- `no in-place overwrite`
- `post-write readback`
- `fail-closed`
- 用户原始存档只读
- 运行时工作区放在程序目录中
- 当前交付以未压缩 release 目录为主

## 8. 非功能要求

### 8.1 平台
- Windows 10 x64，重点环境：Windows 10 25H2
- 验收机器：`ZPX-GE77`
- 杀软：Huorong 开启

### 8.2 交付
- 交付物优先为未压缩 release 目录
- 可执行入口为主程序 `.exe`
- 不要求本阶段额外 zip 打包

### 8.3 可维护性
- 继续沿用 Native C++ 主线
- UI / catalog / preview / write lane 明确分层
- N2A 后不得再依赖示例占位存档驱动主流程

## 9. N2A 执行模型：Ralph + 5 个 subagent

本项目后续不再使用 `$team`；N2A 开始统一采用 `$ralph + subagents`。

### 9.1 Ralph 主 agent 职责
- 冻结接口与写集边界
- 启动 5 个 subagent
- 统一集成、回归、文档更新、阶段 verdict

### 9.2 N2A 的 5 个 subagent scope

| Subagent | 负责对象 | 允许写入范围 | 本阶段必交付物 | 明确不负责 |
|---|---|---|---|---|
| Subagent-1 / Catalog-RecordMap | 统一资产目录 DTO、五类来源建模、徽章/AC 目录适配器接口 | `native/include/ac6dm/contracts/**`、`native/core/emblem/**` 中 catalog DTO 映射、`native/core/ac/**` 新增只读目录代码、`native/tests/*catalog*` | 最小统一目录模型、五类来源分类、AC 只读目录 DTO | 预览渲染、GUI 页面、真实写入 |
| Subagent-2 / Preview | 徽章/AC 预览解析链、preview provenance 枚举与状态映射 | `native/core/preview/**`、`native/core/emblem/**` 中预览支持、`native/core/ac/**` 中只读 preview 探测、`native/tests/*preview*` | 徽章预览链、AC preview provenance 状态、无预览兜底 | 真正的 AC 写入、GUI 总装 |
| Subagent-3 / Emblem-Mutation | 徽章指定目标栏位导入、计划/执行/回读服务 | `native/core/emblem/**` 中 import/apply/readback、`native/tests/*import*` | `将所选徽章导入使用者栏` 的核心服务、目标栏位约束、写后回读 | AC 写入、外部交换文件 |
| Subagent-4 / Exchange-Interface | 未来外部交换格式接口冻结，但本阶段不开放文件交换 | `native/include/ac6dm/contracts/exchange*`、`native/core/exchange/**`、技术文档中的接口章节 | `.ac6emblemdata` / `.ac6acdata` v1 接口草案、GUI disabled reason 契约 | 本阶段真实文件导入导出实现 |
| Subagent-5 / GUI-Validation | 双视图库、目标栏位弹窗、禁用态说明、N2A 验收与证据 | `native/app/**`、`native/resources/ui/**`、`native/tests/ui/**`、`docs/appendices/**` 中 N2A 证据索引 | GUI 双视图、目标栏位选择弹窗、disabled reason、N2A 验收 evidence | 深层 record-map、核心二进制改写 |

### 9.3 N2A 推荐执行顺序
1. Ralph 先冻结统一目录 DTO、preview provenance、write capability、slot 语义。
2. 并行启动：Subagent-1、Subagent-2、Subagent-3、Subagent-5。
3. Subagent-4 只冻结未来交换接口，不开放用户功能。
4. 所有 subagent 返回后，由 Ralph 主 agent 完成：
   - 契约对齐
   - GUI 收口
   - 回归验证
   - 文档与验收证据更新

## 10. N2A 批准执行命令

```text
$ralph "按 docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md、docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md 与 docs/appendices/ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md 执行 N2A。目标：完成统一资产目录壳，并落地徽章稳定产品化 + AC 只读探测。范围：1）徽章与 AC 均按 使用者1/2/3/4 + 分享 五类来源建模；2）GUI 新增徽章库 / AC库双视图；3）实现‘将所选徽章导入使用者栏’并加入目标使用者栏位选择弹窗；4）AC 只做五类来源读出、详情、preview provenance 调查与原生预览链验证，未资格化前禁止 AC 写入；5）引入 preview provenance 与最小统一目录模型；6）所有写操作继续遵守 shadow/staging workspace、fresh-destination-only、no in-place overwrite、post-write readback。执行方式：Ralph + 5 个 subagents；Subagent-1 负责 catalog / record-map，Subagent-2 负责 preview，Subagent-3 负责 emblem mutation，Subagent-4 预留 exchange lane 但本阶段只冻结接口不开放 AC/Emblem 文件交换，Subagent-5 负责 GUI / validation。验收：GUI 可浏览徽章与 AC 的五类来源；徽章可导入指定使用者栏位；AC 至少可读、可分类、可展示预览状态；未资格化功能必须 disabled 且说明原因。"
```
