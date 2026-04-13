# AC6 Data Manager Technical Design

- 状态：Approved Planning Baseline
- 版本：v1.0-baseline
- 最后更新：2026-04-14
- 关联 PRD：`ac6-data-manager.prd.md`
- 关联附录：
  - `appendices/ac6-data-manager.validation-baseline.md`
  - `appendices/ac6-data-manager.release-governance.md`
  - `appendices/ac6-data-manager.adr.md`

## 1. 文档边界 / 非职责

### 1.1 本文档负责什么
本文档定义 AC6 Data Manager 的技术基线，包括系统边界、总体架构、lane 责任、领域模型、AC gate 机制、降级与变更控制规则，是实现与技术评审的主入口。

### 1.2 本文档不负责什么
本文档不重复以下内容：
- 产品目标、用户故事、GUI 叙事与发布承诺全文
- 量化 gate 阈值与完整验证矩阵全文
- 发布治理制度化细则全文

对应权威文档为：
- 产品边界：`ac6-data-manager.prd.md` 第 9 章
- 验证细则：`appendices/ac6-data-manager.validation-baseline.md`
- 治理细则：`appendices/ac6-data-manager.release-governance.md`

## 2. 技术目标与设计原则

### 2.1 已批准技术基线

本项目采用：

**`headless save-domain kernel + late-bound GUI shell`**

其含义是：
- 高风险的存档变更与验证能力放在无界面的内核层
- GUI 壳层仅消费内核输出，不直接耦合容器修改逻辑
- AC 与徽章先分型建模，避免错误抽象固化为系统边界

### 2.2 设计原则

1. **安全优先**：所有 mutation 必须经过 dry-run、restore point、post-write readback、validator 与 rollback。
2. **隔离未证实链路**：已证明的徽章链路可稳定推进，未证明的 AC 写入能力只能 gated 开放。
3. **分型而非过早统一**：`EmblemAsset` 与 `AcAsset` 在语义未充分证明前，不统一 editable/dependency/package 语义。
4. **正文讲基线，附录讲判定**：技术正文记录架构与边界，验证附录记录具体阈值与矩阵。

## 3. 系统上下文与边界

### 3.1 输入
- `.sl2` 存档文件
- 本程序定义的分享包
- 本地配置与缓存
- 外部工具链（WitchyBND）

### 3.2 输出
- 修改后的存档副本
- restore point
- 审计日志
- 兼容性报告
- incident artifact
- 预览缓存与库索引

### 3.3 外部依赖
- WitchyBND：用于容器解包/回包
- 本地文件系统：用于工作区、备份与缓存
- 图像处理能力：用于预览提取与 fallback

### 3.4 系统边界
系统负责：
- 存档读取、分析、导入模拟、写入、校验、回滚
- GUI 浏览与风险展示
- 包导入/导出

系统不负责：
- 在线社区平台本体
- 游戏运行时注入
- 云端同步

## 4. 总体架构

### 4.1 Container / Workspace Lane
负责：
- WitchyBND 受控适配
- shadow workspace 管理
- 解包、回包、副本验证、再覆盖
- 工具链版本锁定与环境检查

关键限制：
- 只允许在 shadow workspace 进行变更
- 不允许原地覆盖原始存档
- 工具链版本必须固定

### 4.2 Save-domain Kernel
负责：
- 解析 `EmblemAsset` 与 `AcAsset`
- 维护 catalog、slot map、dependency graph
- 输出 ImportPlan、DeleteImpactPlan、CompatibilityReport、AuditEntry

### 4.3 Safety / Validation Lane
负责：
- dry-run
- restore point
- post-write readback
- reference graph validator
- binary invariant checker
- rollback orchestration
- fail-closed policy

### 4.4 Preview Lane
负责：
- 徽章预览提取
- AC 预览来源分析
- fallback renderer 或占位策略
- 缩略图缓存

### 4.5 Package Lane
负责：
- `ac6emblempkg` v1
- `ac6acpkg` experimental v0
- manifest、checksum、兼容性声明、依赖列表

### 4.6 GUI Lane
负责：
- 资产浏览
- 风险与依赖可视化
- 导入模拟 UI
- 删除影响展示
- 回滚入口与审计查看

GUI 不直接访问底层 mutation 逻辑，只通过内核输出能力工作。

## 5. 领域模型

### 5.1 EmblemAsset
语义：已由现有链路强证明的徽章对象。

典型属性：
- asset id
- slot id
- origin
- editable state
- preview handle
- dependency references

### 5.2 AcAsset
语义：AC 数据对象；其 mutation 能力受 gate 控制。

典型属性：
- record id
- slot id
- origin bucket
- preview strategy
- dependency references
- editability confidence

### 5.3 薄共享接口
仅共享：
- `CatalogItem`
- `PreviewHandle`
- `MutationPlanSummary`
- `AuditEntry`

不共享：
- editable 语义完整定义
- package payload 细节
- slot 迁移规则
- 依赖补齐行为

### 5.4 关键对象
- `ImportPlan`
- `DeleteImpactPlan`
- `RestorePoint`
- `CompatibilityReport`
- `AuditEntry`

## 6. Lane 设计细节

### 6.1 Container / Workspace
建议模块：
- `WitchyBndAdapter`
- `SaveWorkspace`
- `PackTransaction`

关键流程：
1. 打开存档
2. 建立 shadow workspace
3. 生成 restore point
4. 执行 dry-run 或 mutation
5. 回包
6. 回读验证
7. 通过后才替换目标副本

### 6.2 Save-domain Kernel
建议模块：
- `CatalogBuilder`
- `DependencyResolver`
- `SlotMapService`
- `ImportPlanner`
- `DeleteImpactAnalyzer`

核心职责是把“原始二进制状态”转换成“用户可理解、可验证、可执行的计划对象”。

### 6.3 Safety / Validation
建议模块：
- `BackupService`
- `InvariantChecker`
- `ReferenceGraphValidator`
- `RollbackService`
- `AuditService`

### 6.4 Preview
建议策略：
- 优先直接提取
- 无法提取时按 gate 结果决定 fallback 或占位
- 预览来源未闭合时不得暗中伪装为稳定图像能力

### 6.5 Package
策略：
- 徽章包早期即可稳定化
- AC 包必须保持 experimental v0，直到 gate 与治理要求满足

### 6.6 GUI
建议布局：
- 左侧导航
- 顶部状态条
- 中央列表/卡片主区
- 右侧详情区

关键 UI 实体：
- 风险标识
- 依赖可视化
- ImportPlan/ DeleteImpactPlan 弹层
- 回滚入口
- 审计查看器

## 7. AC Gate 机制

### 7.1 Gate 目标
AC 相关写入能力不能作为自然里程碑自动开启，而必须由证据驱动 gate 解锁。

### 7.2 Gate 与能力解锁关系
- `record map proven`：允许更深入的只读分析
- `dependency graph proven`：允许 delete/import dry-run 原型
- `editability transition proven`：允许 experimental import
- `preview provenance proven`：允许冻结 AC 浏览预览策略
- `round-trip corpus proven`：允许 experimental lane 作为受控能力运行

### 7.3 Gate 对 stable / experimental 的控制边界
- Stable 不依赖 AC mutation gate 全通过
- Experimental 必须依赖 5 个 gate 全通过
- Stable / Experimental 的产品级事实来源以 PRD 第 9 章为准

量化阈值详见：
- `appendices/ac6-data-manager.validation-baseline.md`

## 8. Shell alternatives 与冻结规则

### 8.1 当前偏好
当前 GUI 壳层偏好：Qt/QML。

理由：
- 有利于与 C++ 内核直接协作
- 更适合高质量桌面 GUI 与本地图像/文件系统集成

### 8.2 备选方案
- C++/Rust kernel + Tauri/WebView shell
- C++ kernel + 轻量原生壳（仅内部验证工具）

### 8.3 Freeze 条件
只有同时满足以下条件，才冻结 GUI 壳层：
1. M1 完成
2. `preview provenance proven` 已有明确策略分派
3. 领域 API 连续 2 轮原型无破坏性变更
4. 已完成一轮 Data / Visual Fidelity 试评

### 8.4 Freeze review 产物
详见：
- `appendices/ac6-data-manager.release-governance.md`

## 9. 稳定性、降级与能力分级

### 9.1 Stable Lane
- Emblem stable mutation
- AC read-only explorer
- 备份 / 回滚 / 审计 / 兼容性提示

### 9.2 Experimental Lane
- AC 导入
- AC 包导入/导出
- AC 删除、替换、依赖补齐类 mutation

### 9.3 自动降级条件
当以下任一情况不闭合时，系统必须降级为：
- Emblem stable + AC read-only

关键降级触发包括：
- preview provenance 未闭合
- editability transition 未闭合
- dependency confidence 低于高阈值

### 9.4 Fail-closed 原则
任何高风险异常都必须阻断 Apply，而不是默认继续：
- 工具版本不匹配
- 回读失败
- validator 失败
- dependency confidence 不足
- rollback 不可保证

## 10. 变更控制与演进规则

### 10.1 冻结基线
以下内容属于当前冻结基线：
- 内核优先于壳层
- Emblem / AC 分型建模
- Stable / Experimental 双边界
- gate 驱动 AC mutation
- fail-closed

### 10.2 可演进部分
以下内容可在后续执行中演进：
- GUI 视觉实现细节
- fallback renderer 具体实现
- AC experimental v0 package schema 细节
- 风险规则库条目

### 10.3 Experimental -> Stable 升级条件
AC 从 experimental 升 stable 的治理条件详见：
- `appendices/ac6-data-manager.release-governance.md`

## 11. 关联附录

- 验证基线：`appendices/ac6-data-manager.validation-baseline.md`
- 发布治理：`appendices/ac6-data-manager.release-governance.md`
- ADR：`appendices/ac6-data-manager.adr.md`

## 12. 参考依据

### 本地实现依据
- `external/ACVIEmblemCreator/gui/src/EmblemImport.cpp`
- `external/ACVIEmblemCreator/gui/src/main.cpp`
- `ACVIEmblemCreator_实现机制分析.md`

### 外部依据
- AC6 1.06 Patch Notes：<https://store.steampowered.com/news/app/1888160/view/4016716468489779702>
- WitchyBND：<https://github.com/ividyon/WitchyBND>
- ACVIEmblemCreator issue #13：<https://github.com/pawREP/ACVIEmblemCreator/issues/13>
- AC Companion About：<https://ac-companion.app/about>

## 13. 执行命令与衔接规则

### 13.1 并行启动命令

计划的首个执行阶段必须使用以下 `$team` 命令或等价语义命令，范围限定为 M0、M0.5、M1：

```text
$team "按 docs/ac6-data-manager.prd.md、docs/ac6-data-manager.technical-design.md、docs/appendices/ac6-data-manager.validation-baseline.md 执行第一阶段，只做 M0、M0.5、M1。Worker1 负责 container/workspace + WitchyBND adapter + restore point + post-write readback；Worker2 负责 emblem catalog / share->user 手动选定导入 / ac6emblempkg v1；Worker3 负责 validation baseline / golden corpus / rollback / audit；Worker4 负责 GUI shell prototype（emblem library、ImportPlan、DeleteImpactPlan、audit view）；Verifier 负责按 validation-baseline 复核 gate 与验收。禁止开启任何 AC mutation；AC 仅允许 read-only baseline 与 reverse artifacts 整理。所有 mutation 必须先 dry-run，再在 shadow workspace apply，再 post-write readback；禁止 in-place overwrite；失败必须 fail-closed 并保留 incident artifact。"
```

### 13.2 串行收口命令

`$team` 完成后，必须用以下 `$ralph` 命令或等价语义命令做统一整合与验收：

```text
$ralph "基于 $team 第一阶段产出做串行收口，只整合 M0、M0.5、M1，不开启 AC experimental import。任务：1）合并 container、emblem、validation、GUI 原型成果；2）修正契约不一致与遗留缺陷；3）完成 Emblem stable lane 端到端验证、回读验证、回滚演练与审计检查；4）更新 docs/ac6-data-manager.prd.md、docs/ac6-data-manager.technical-design.md 与 appendices 中的实施状态和证据索引；5）产出 M2 准入报告，明确 AC read-only explorer 的 record-map、dependency、preview-provenance 下一步工作。验收：emblem round-trip 100%，rollback 100%，audit 完整，AC 仍保持 read-only，所有 mutation 继续遵守 shadow workspace、no in-place overwrite、dry-run before apply、fail-closed。"
```

### 13.3 衔接规则

1. `$team` 阶段不允许以任何形式解锁 AC experimental mutation。
2. `$ralph` 阶段的目标是整合与验收，而不是扩大范围。
3. 只有在后续 gate 证据闭合并重新通过评审后，才能为 AC experimental import 制定新的执行命令。
