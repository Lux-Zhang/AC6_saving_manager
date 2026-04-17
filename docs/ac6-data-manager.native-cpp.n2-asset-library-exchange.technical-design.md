# AC6 Data Manager Native C++ N2 Technical Design：Asset Library & Exchange

- 文档状态：Approved / N2A-N2D Code Landed / Qualification Evidence Reopened
- 版本：v2.0-native-cpp-n2
- 最后更新：2026-04-15（N2B/N2C/N2D docs-evidence lane 收口）
- 关联 PRD：`docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联验收基线：`docs/appendices/ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`

## 1. 设计目标

N2 的技术目标不是一次性同时开放“徽章 + AC”的所有写入，而是在现有 Native 基线上分层推进：
- N2A：先建立统一资产目录壳，完成徽章 stable 产品化，并把 AC 拉入只读目录
- N2B：补齐徽章文件交换
- N2C：完成 AC 资格认证
- N2D：最后开放 AC 文件交换与指定栏位写入

当前执行范围已覆盖 `N2A/N2B/N2C/N2D` 的部分代码实现；但用户于 2026-04-15 明确指出“每个使用者下有很多 AC 存档”，因此 AC 资格与最终收口证据必须回退重审。

## 1.1 当前实施状态

### N2A 已落地能力
- 统一资产目录 DTO 已落地。
- `EmblemCatalogAdapter` 与 `AcCatalogAdapter` 的分域策略已成立。
- `TargetUserSlotDialog` 已作为 N2A 徽章导入主路径 UI。
- preview 四态已落地到合同与测试。
- exchange future-action / disabled-reason 合同已冻结。

### N2B/N2C/N2D 当前技术状态
- `N2B`：`.ac6emblemdata` 的 parser / serializer / import-export 链路已进入当前构建与测试基线，并由 package codec fail-closed 约束校验。
- `N2C`：当前自动化测试只覆盖“把 `USER_DATA002..006` 直接视为五个 top-level payload”的 provisional record-map；由于真实每个使用者下可能有很多 AC 记录，当前 gate 已重开。
- `N2D`：`.ac6acdata` 与 AC 用户可见动作虽已接线到当前 GUI / workflow，但在真实多 AC record model 未澄清前，不得宣称 qualification 完成。

### 1.2 新发现的 blocker（2026-04-15）
- 当前 native AC 目录实现把 `USER_DATA002..006` 直接当成 `user1..user4/share` 的 5 条 AC 记录；用户明确指出每个使用者下实际存在很多 AC 存档，因此现有 AC catalog / gate 只能代表“top-level payload 假设”。
- 当前徽章预览实现只稳定支持 `SquareSolid(100)` 与 `EllipseSolid(102)` 两类 decal；对其他 `decalId` 仍以占位图表示。`ACVIEmblemCreator` 可借鉴其坐标缩放、图元叠加和 mask preview 逻辑，但它同样不是任意游戏 decalId 的全量映射表。
- 结论：`N2B` 的 exchange 主链可继续沿用，但 `N2C/N2D` 不得再以当前 AC record-map / 徽章预览质量宣称最终完成，必须补做多 AC record parser 与 shape-id 映射。

## 2. 继承基线

直接继承已完成能力：
- 真实 `.sl2` 打开成功
- WitchyBND silent mode + GUI 稳定调用
- 程序目录下 runtime workspace
- Emblem stable lane 的单分享徽章导入闭环
- release 目录 sidecar 契约

N2A 不重做以下底层约束，而是在其上扩展：
- `fresh-destination-only`
- `shadow / staging workspace`
- `post-write readback`
- `no in-place overwrite`
- `fail-closed`

## 3. 总体架构

```text
native/
  app/
    main.cpp
    MainWindow.*
    home_library_view.*
    asset_library_tabs.*              # N2A 新增：徽章库 / AC库双视图壳
    target_slot_dialog.*              # N2A 新增：目标使用者栏位选择弹窗
  core/
    emblem/
      emblem_domain.*
      emblem_preview_service.*        # N2A 扩展
      emblem_import_service.*         # N2A 扩展：指定目标栏位
    ac/
      ac_catalog_service.*            # N2A 新增：只读目录与五类来源
      ac_preview_probe_service.*      # N2A 新增：preview provenance 探测
      ac_record_ref.*                 # N2A 新增：只读 record_ref
    preview/
      preview_types.*                 # N2A 新增：通用 preview provenance / state
      preview_resolver.*              # N2A 新增：通用预览解析结果
    exchange/
      exchange_contracts.*            # N2A 新增：只冻结接口，不开放功能
    workspace/
      SessionWorkspace.*
      ReadbackVerifier.*
  include/ac6dm/contracts/
    asset_catalog.hpp                 # N2A 新增：最小统一目录模型
    preview.hpp                       # N2A 新增
    exchange.hpp                      # N2A 新增
    service_interfaces.hpp            # N2A 扩展
  tests/
    asset_catalog_contract_test.cpp
    ac_catalog_probe_test.cpp
    preview_contract_test.cpp
    target_slot_dialog_test.cpp
```

## 4. 统一目录模型

N2A 引入通用资产 DTO：

```text
AssetCatalogItemDto
- assetKind                # emblem | ac
- sourceBucket             # user1 | user2 | user3 | user4 | share
- slotIndex                # 0..3 for user buckets, null for share if needed
- displayName
- previewState             # native_embedded | derived_render | unavailable | unknown
- writeCapability          # editable | read_only | locked_pending_gate
- recordRef                # 底层定位引用，不直接暴露给普通用户
- detailProvenance         # 详情来自哪个解析链
- assetId                  # UI 和服务内部稳定 ID
```

设计目的：
1. 让 `徽章` 与 `AC` 在 GUI 列表层可以复用同一套显示与过滤逻辑。
2. 让 preview、disabled reason、详情面板拥有统一语义。
3. 让后续 N2B/N2D 的外部交换格式可以直接引用稳定的 `recordRef + assetKind + sourceBucket` 描述。

## 5. 五类来源建模

## 5.1 SourceBucket 定义

```text
user1
user2
user3
user4
share
```

### 设计说明
- `user1..user4` 对应玩家在游戏中的四个使用者栏位。
- `share` 对应分享栏位集合。
- GUI 不再只显示 `user / share` 二分，而必须精确到四个使用者栏位。

## 5.2 SlotIndex 语义

- 对 `user1..user4`：`slotIndex` 固定对应 0..3
- 对 `share`：
  - 若底层本身是 share 集合中的离散记录，可额外在 `recordRef` 中保存 share 内部索引
  - GUI 主分类仍只显示为 `分享`

## 6. 目录适配器分层

N2A 使用两个显式适配器，而不是把双域逻辑硬塞进现有 emblem 服务：

### 6.1 `EmblemCatalogAdapter`
职责：
- 从 save 解包结果中提取徽章记录
- 映射到五类来源
- 提供详情摘要、预览句柄、可写能力
- 向 GUI 输出 `AssetCatalogItemDto(assetKind=emblem)`

### 6.2 `AcCatalogAdapter`
职责：
- 从 save 解包结果中提取 AC 记录
- 建立 `recordRef`
- 输出五类来源的目录项
- 在 N2A 只提供只读详情与 preview provenance，不提供写入
- 向 GUI 输出 `AssetCatalogItemDto(assetKind=ac)`

### 6.3 不在 N2A 合并为单一“万能解析器”的原因
- 徽章与 AC 的 record-map、预览来源、可写资格差异很大
- 统一的是 GUI 和 DTO，不是底层解析器实现
- 分离适配器可以避免 N2A 过早把 AC 写入风险带入 stable lane

## 7. Preview 体系

## 7.1 PreviewState 定义

统一采用四态：
- `native_embedded`
- `derived_render`
- `unavailable`
- `unknown`

## 7.2 徽章预览策略

优先级：
1. 若存档中存在原生徽章预览或稳定可定位的位图资源，记为 `native_embedded`
2. 若无原生预览，但可以根据 emblem 数据进行稳定渲染，记为 `derived_render`
3. 若无可用链路，记为 `unavailable`
4. 若链路仍未确认，记为 `unknown`

N2A 要求：
- 至少把当前徽章预览状态显式建模并在 GUI 展示
- 若仍需衍生渲染，可先采用稳定占位链，但必须标记 provenance

## 7.3 AC 预览策略

N2A 只做探测与 provenance 建模：
1. 优先尝试使用 save 中的原生预览图片
2. 若能稳定读取，标记 `native_embedded`
3. 若尚不能确认其来源，标记 `unknown`
4. 如确认当前样本没有可用预览，标记 `unavailable`

N2A 明确不做：
- 把 `unknown` 伪装成已经稳定的 AC 预览
- 在 preview provenance 未闭合前开放 AC 写入

## 8. N2A GUI 设计

## 8.1 顶层结构

主窗口切换为：
- 顶部：打开存档、刷新、状态摘要
- 主区域：`徽章库` 与 `AC库` 双标签或双侧边入口
- 右侧：详情与预览
- 动作区：按当前资产类型展示允许的动作

## 8.2 徽章库

必须提供：
- 五类来源过滤
- 列表浏览
- 详情
- 预览
- `将所选徽章导入使用者栏`
- `导入徽章文件到使用者栏`、`导出所选徽章` 的 disabled 占位与原因（N2A 仍禁用）

## 8.3 AC 库

必须提供：
- 五类来源过滤
- 列表浏览
- 详情
- 预览或 preview provenance
- `将所选AC导入使用者栏` disabled 占位与原因
- `导入AC文件到使用者栏`、`导出所选AC` disabled 占位与原因

## 8.4 目标栏位选择弹窗

新建 `TargetUserSlotDialog`，统一用于未来徽章 / AC 两种导入。

弹窗字段：
- 当前资产名称
- 来源类别
- 目标使用者栏位列表（1/2/3/4）
- 每个栏位状态：空闲 / 已占用 / 当前阶段不可写 / 将阻断
- 推荐栏位
- 明确按钮：`确认导入`
- 取消按钮

N2A 要求：
- 徽章导入必须实际使用该弹窗
- AC 只需为未来复用冻结 DTO 和 UI 壳，不执行写入

## 9. N2A 服务设计

## 9.1 Open Save

```text
打开存档
-> 解包 staging copy
-> EmblemCatalogAdapter 输出徽章目录
-> AcCatalogAdapter 输出 AC 目录
-> SessionState 持有双域目录快照
-> GUI 刷新双视图
```

## 9.2 Emblem 指定目标栏位导入

```text
用户在徽章库选中一项
-> 点击“将所选徽章导入使用者栏”
-> TargetUserSlotDialog 打开
-> 用户选择 user1..user4 目标栏位
-> EmblemImportPlanningService 校验：
   - 来源必须为 share
   - 目标栏位必须在当前 N2A 规则下允许写入
   - 不允许违反 no in-place overwrite / staging / fresh destination 约束
-> EmblemApplyService 修改 staging 内 save
-> post-write readback
-> 通过后输出 fresh destination
-> 重新打开输出结果，刷新目录快照
```

## 9.3 AC 只读探测

```text
打开存档
-> AcCatalogAdapter 列出五类来源
-> AcPreviewProbeService 给出 preview provenance
-> GUI 展示只读详情
-> 所有 AC 写入动作保持 disabled
```

## 10. 交换格式接口冻结（N2A 只冻结，不实现）

本阶段冻结接口，避免后续 N2B / N2D 再次大改 GUI 与 DTO。

### 10.1 徽章交换格式 `.ac6emblemdata`
预留字段：
- format_version
- asset_kind = emblem
- source_metadata
- preview_metadata
- record_payload
- compatibility_info

### 10.2 AC 交换格式 `.ac6acdata`
预留字段：
- format_version
- asset_kind = ac
- source_metadata
- preview_metadata
- record_payload
- compatibility_info

### 10.3 最小合同冻结落点
N2A 新增只读合同头：
- `native/include/ac6dm/contracts/exchange_contracts.hpp`

以及只返回静态合同描述的实现层：
- `native/core/exchange/exchange_contracts.hpp`
- `native/core/exchange/exchange_contracts.cpp`

本层职责仅限于：
1. 冻结 `.ac6emblemdata` 与 `.ac6acdata` 的最小字段集合。
2. 向 GUI 提供 future action / disabled reason 合同。
3. 明确本阶段 `implementationStatus = contract_frozen_only`。

本层明确不做：
- 真实文件读写
- 真实序列化 / 反序列化
- 真实导入导出执行
- 任何 AC mutation

### 10.4 Future Action / Disabled Reason 合同
为避免 GUI 在 N2B/N2D 再次重构动作栏，N2A 先冻结 `FutureActionContract`：
- `actionId`：稳定动作 ID
- `label`：用户文案，例如 `导入徽章文件到使用者栏`
- `availability`：当前是否可用；N2A 相关 future action 必须为 `Disabled`
- `disabledReasonCode`：程序可判定的稳定 reason code
- `disabledReasonText`：GUI 直接展示给普通用户的原因
- `unlockStage`：该动作预计在哪个阶段开放
- `implementationStatus`：当前阶段必须为 `contract_frozen_only`
- `policyNote`：补充限制，例如 `AC gate 未通过前禁止 mutation`

N2A 固定输出的 disabled reason 至少包括：
- 徽章文件交换：`future.exchange.emblem.deferred.until-n2b`
- AC 用户栏导入 / AC 文件交换：`future.exchange.ac.locked.pending-gate`

### 10.5 GUI 对接要求（接口冻结，不改总装）
当 GUI 在后续阶段接入本合同后，必须遵守：
1. `导入徽章文件到使用者栏` / `导出所选徽章`：在 N2A 显示 disabled，并使用徽章交换 deferred reason。
2. `将所选AC导入使用者栏` / `导入AC文件到使用者栏` / `导出所选AC`：在 N2A 显示 disabled，并使用 AC gate locked reason。
3. N2A 禁止将 disabled action 伪装为可点击但失败的假动作；应在点击前即显示原因。
- asset_kind = ac
- source_metadata
- preview_metadata
- record_payload
- dependency_metadata
- compatibility_info

### 10.3 N2A 明确不做
- 不把上述格式暴露给用户真实导入导出
- 不写最终二进制封装细节
- 不承诺跨版本兼容以外的稳定性

## 11. 与现有底层约束的关系

N2A 不改变：
- WitchyBND 调用方式仍由当前 adapter 层统一负责
- 运行时工作区继续位于程序目录 `runtime/workspaces`
- 所有写入继续走 shadow/staging/fresh-destination-only
- 失败继续 fail-closed

N2A 改变：
- 目录模型从“单徽章 catalog”升级为“统一资产目录壳”
- GUI 从“单库 + 旧按钮”升级为“双域 + 正常用户动作词 + disabled future actions”
- 徽章导入从“默认挑第一个 share”升级为“用户选中资产 + 目标栏位选择”

## 12. N2A subagent 写集边界

### Subagent-1 / Catalog-RecordMap
- 写集：
  - `native/include/ac6dm/contracts/asset_catalog.hpp`
  - `native/include/ac6dm/contracts/service_interfaces.hpp`
  - `native/core/emblem/*catalog*`
  - `native/core/ac/ac_catalog_service.*`
  - `native/tests/*catalog*`
- 不得写：GUI 页面、preview 渲染、exchange 实现

### Subagent-2 / Preview
- 写集：
  - `native/core/preview/**`
  - `native/core/emblem/*preview*`
  - `native/core/ac/*preview*`
  - `native/tests/*preview*`
- 不得写：用户弹窗、真实写入逻辑

### Subagent-3 / Emblem-Mutation
- 写集：
  - `native/core/emblem/*import*`
  - `native/core/emblem/*apply*`
  - `native/core/emblem/*readback*`
  - `native/tests/*import*`
- 不得写：AC 写入、GUI 大页面

### Subagent-4 / Exchange-Interface
- 写集：
  - `native/include/ac6dm/contracts/exchange.hpp`
  - `native/core/exchange/**`
  - N2 技术文档中的接口章节
- 不得写：真实文件导入导出实现、GUI 真实启用逻辑

### Subagent-5 / GUI-Validation
- 写集：
  - `native/app/**`
  - `native/resources/ui/**`
  - `native/tests/ui/**`
  - `docs/appendices/*n2*`
- 不得写：底层 record-map 算法、核心 mutation 逻辑

### Ralph 主 agent
- 写集：跨模块 glue code、最终接口对齐、文档收口
- 不得长期接管任一 subagent 的完整 lane

## 13. 验证策略

N2A 至少需要以下验证：
1. 打开真实存档 -> 双域目录可见
2. 徽章五类来源显示正确
3. AC 五类来源显示正确
4. 徽章预览与详情可见
5. AC 详情与 preview provenance 可见
6. `将所选徽章导入使用者栏` 通过目标栏位弹窗完成
7. AC 写入按钮保持 disabled，并显示原因
8. 写入结果通过 readback
9. 原始存档零副产物

## 14. N2A Ralph 执行命令

```text
$ralph "按 docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md、docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md 与 docs/appendices/ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md 执行 N2A。目标：完成统一资产目录壳，并落地徽章稳定产品化 + AC 只读探测。范围：1）徽章与 AC 均按 使用者1/2/3/4 + 分享 五类来源建模；2）GUI 新增徽章库 / AC库双视图；3）实现‘将所选徽章导入使用者栏’并加入目标使用者栏位选择弹窗；4）AC 只做五类来源读出、详情、preview provenance 调查与原生预览链验证，未资格化前禁止 AC 写入；5）引入 preview provenance 与最小统一目录模型；6）所有写操作继续遵守 shadow/staging workspace、fresh-destination-only、no in-place overwrite、post-write readback。执行方式：Ralph + 5 个 subagents；Subagent-1 负责 catalog / record-map，Subagent-2 负责 preview，Subagent-3 负责 emblem mutation，Subagent-4 预留 exchange lane 但本阶段只冻结接口不开放 AC/Emblem 文件交换，Subagent-5 负责 GUI / validation。验收：GUI 可浏览徽章与 AC 的五类来源；徽章可导入指定使用者栏位；AC 至少可读、可分类、可展示预览状态；未资格化功能必须 disabled 且说明原因。"
```

## 11. N2B / N2C / N2D 文档与资格化收口

### 11.1 N2B：徽章交换 v1 证据要求
N2B 必须补齐以下文档化证据：
- `.ac6emblemdata` 字段示例与版本号
- export 成功记录
- import 到指定 `user1..user4` 的成功记录
- 失败样本记录：坏扩展名、坏校验、跨版本不兼容、目标栏位非法
- GUI 主流程截图：
  - `导出所选徽章`
  - `导入徽章文件到使用者栏`
  - 目标栏位弹窗

### 11.2 N2C：AC Gate 记录要求
N2C 不能只写“通过/未通过”，必须为每个 gate 记录：
- 样本集范围
- 自动化验证命令
- 人工观察项
- 通过条件
- 实测结果
- blocker / 结论 / 下一步

推荐统一使用：
- `docs/appendices/ac6-data-manager.native-cpp.n2c-ac-gate-record.md`

### 11.3 N2D：AC exchange 与最终 GUI 收口证据
N2D 必须记录：
- `.ac6acdata` 格式实例与兼容字段
- AC export / import / re-open / readback 闭环
- GUI 中 AC 与 emblem 动作分域后仍不混淆
- final release checklist 与已知限制

### 11.4 文档写集约束
本轮 docs/evidence lane 只维护 `docs/**`。
任何生产代码实现、接口修正、测试修正均应由对应 implementation lane 完成，文档侧不得通过改文档掩盖 gate 未通过事实。
