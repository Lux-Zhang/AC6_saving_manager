# AC6 Data Manager PRD

- 状态：Approved Planning Baseline
- 版本：v1.0-baseline
- 最后更新：2026-04-14
- Owner：待实现阶段指定
- 关联文档：
  - `ac6-data-manager.technical-design.md`
  - `appendices/ac6-data-manager.validation-baseline.md`
  - `appendices/ac6-data-manager.release-governance.md`
  - `appendices/ac6-data-manager.adr.md`

## 1. 文档边界 / 非职责

### 1.1 本文档负责什么
本文档用于定义 AC6 Data Manager 的产品边界、目标用户、功能范围、交付里程碑、发布边界与验收摘要，是后续实现、评审与发版的产品级基线。

### 1.2 本文档不负责什么
本文档不承载以下内容的完整细节：
- 5 个 AC gate 的量化阈值全文
- 完整验证矩阵、大型验收表、故障注入细则
- 发布治理的完整制度化约束
- ADR 的完整决策推导

上述内容分别以以下附录为准：
- 验证基线：`appendices/ac6-data-manager.validation-baseline.md`
- 发布治理：`appendices/ac6-data-manager.release-governance.md`
- ADR：`appendices/ac6-data-manager.adr.md`

## 2. 背景与问题定义

《ARMORED CORE VI FIRES OF RUBICON》提供了 AC Data、Image Data、Share ID 等能力，但原生体验存在以下限制：

1. 下载的分享徽章和分享 AC 存在编辑限制，玩家无法方便地把感兴趣的内容转成自己的可编辑版本。
2. 原生界面对本地资产的管理能力有限，缺少面向重度收藏与整理的搜索、筛选、标签、收藏、批量分析能力。
3. AC、徽章、decal、image data 之间存在引用关系，删除或迁移错误可能导致资源失效、显示退回默认样式，甚至引发菜单异常或读档风险。
4. 现有第三方工具大多只覆盖局部能力，例如只处理徽章导入，或缺少事务式写回、自动回滚、审计与风险提示。

本项目要解决的问题不是“再做一个脚本型导入器”，而是构建一个完整、美观、可回滚、可审计的本地资产管理器，兼顾用户体验与存档安全。

## 3. 产品目标与非目标

### 3.1 产品目标

本产品的目标如下：

1. 提供完整 GUI，使用户可以像在游戏中一样浏览 AC 与徽章资产。
2. 支持手动选定分享徽章并导入“使用者”栏，导入后可继续编辑。
3. 支持手动选定分享 AC 并导入“使用者”栏，导入后可继续编辑，但仅在相关技术门槛通过后开放。
4. 支持徽章与 AC 的本地打包分享、导入、兼容性检查与依赖分析。
5. 提供存档级安全保障，包括自动备份、导入模拟、写后回读、失败回滚与审计日志。
6. 为后续团队执行提供稳定的产品边界与发版口径，避免功能范围漂移。

### 3.2 非目标

当前阶段不把以下事项作为稳定承诺：

1. 承诺所有分享 AC 都能稳定转为可编辑 user AC。
2. 承诺所有 AC 都一定具备高质量图像预览。
3. 承诺 AC 分享包立即成为稳定 v1 格式。
4. 承诺跨平台桌面壳层立即冻结；当前仅确定内核与门槛策略。
5. 承诺覆盖游戏存档之外的云端社区、在线同步或多人协作平台。

## 4. 用户与核心使用场景

### 4.1 目标用户

1. **收藏型玩家**：想在本地整理大量 AC / 徽章资源，要求浏览效率高、检索方便。
2. **创作型玩家**：希望把下载资源转为自己的可编辑版本，并进行二次创作。
3. **分享型玩家**：希望将自己的徽章或 AC 打包分享给其他使用本程序的玩家。
4. **谨慎型玩家**：对存档安全敏感，希望任何改动都有备份、验证与回滚。

### 4.2 核心使用场景

#### 场景 A：分享徽章转为自有可编辑资产
用户在分享徽章列表中浏览、筛选并手动选定目标徽章，查看预览与导入模拟后，一键导入到自己的 user 区，并在游戏中继续编辑。

#### 场景 B：本地 AC 资产浏览与风险分析
用户打开存档后，按来源、风险、依赖情况浏览 AC 列表，查看预览图或预览策略说明，理解哪些资源可以安全删除、哪些资源依赖其他对象。

#### 场景 C：AC 资源实验导入
在 AC 技术门槛全部通过后，用户将选定的 share AC 以实验模式导入为 user AC；系统在导入前展示 ImportPlan，在导入后强制执行验证与可回滚流程。

#### 场景 D：资源打包分享
用户将一个徽章或 AC 导出为本程序定义的分享包，另一位用户导入时先看到兼容性报告、依赖补齐计划和风险提示，再决定是否写入自己的存档。

## 5. 产品范围

### 5.1 Stable 范围

Stable 范围是当前产品基线的单一事实来源。

当前稳定承诺包含：

1. 打开 `.sl2` 存档并建立安全工作区。
2. 浏览用户徽章与分享徽章。
3. 预览徽章图片。
4. 手动选定分享徽章并导入到 user 栏，导入后保持可编辑。
5. 徽章分享包 v1 的导入与导出。
6. AC 只读浏览：
   - 分类
   - 来源标记
   - 风险标签
   - 依赖分析（仅展示已证实置信度）
   - 预览图或预览策略说明
7. 自动备份、导入模拟、写后回读、回滚与审计日志。
8. 删除影响分析（仅限已证实对象和已证实依赖关系）。

### 5.2 Experimental 范围

以下能力仅在 experimental 范围开放：

1. share AC -> user AC 导入。
2. AC 分享包 experimental v0 的导入与导出。
3. AC 删除、替换、依赖补齐类写入能力。
4. 针对 AC 的自动修复或依赖补齐尝试。

Experimental 范围必须受以下条件约束：
- 仅在 AC 5 个量化 gate 全部通过后开放。
- 仅在实验模式下可用。
- 必须执行双备份、dry-run、写后回读和验证清单。

### 5.3 Out of Scope

以下内容当前明确不在范围内：

1. 云端同步、在线账户系统、在线分享平台本体。
2. 游戏内实时热更新或运行时注入。
3. 非 AC6 存档格式支持。
4. 未经 gate 证明的 AC stable 写入承诺。
5. 任意补丁版本下无差别兼容的保证。

## 6. 功能需求

### 6.1 存档接入

系统必须支持：
- 选择 `.sl2` 存档
- 检查外部工具可用性
- 建立 shadow workspace
- 生成 restore point
- 读取基础元数据并输出风险提示

### 6.2 徽章管理

系统必须支持：
- 浏览 user/share 徽章
- 缩略图与详情预览
- 按标签、来源、最近导入排序与筛选
- 手动选定分享徽章导入到 user 栏
- 导入前展示 ImportPlan
- 导入后可继续编辑
- 导出与导入 `ac6emblempkg` v1

### 6.3 AC 管理

系统必须支持：
- 浏览 user/share AC
- 展示来源、分类、风险和依赖情况
- 支持预览图或预览策略占位说明
- 在只读阶段提供依赖和风险分析
- 在 experimental 阶段提供经 gate 解锁后的导入能力

### 6.4 安全与恢复

系统必须支持：
- 自动备份
- 导入模拟 Dry-run
- 写后回读
- 二进制不变量校验
- 引用一致性校验
- 失败自动阻断
- 一键回滚
- 审计日志查看

### 6.5 本地库体验

系统必须支持：
- 搜索
- 标签
- 收藏
- 最近导入
- 列表/卡片切换
- 批量导入模拟
- 删除影响分析

## 7. GUI / UX 基线

### 7.1 视觉方向

界面采用深色工业机库风格，强调：
- 高信息密度
- 清晰层级
- 风险状态醒目
- 预览优先
- 接近游戏心智，但不要求像素级复刻

### 7.2 信息架构

建议界面结构如下：
- 左侧导航：存档总览、AC、徽章、分享库、导入导出、备份与恢复、审计日志、设置
- 顶部状态条：当前存档、模式、备份状态、工具链状态、风险提示
- 中央主区：卡片/列表双模式资产浏览
- 右侧详情区：大图预览、元数据、依赖、操作入口、风险说明

### 7.3 核心交互基线

1. 分享徽章导入必须是“手动选定 -> ImportPlan -> Apply -> 写后验证”。
2. AC 导入必须是“手动选定 -> 依赖分析 -> 风险确认 -> 实验模式 Apply -> 验证清单”。
3. 删除动作必须先显示 DeleteImpactPlan。
4. 所有写入动作必须能在 UI 中看到 restore point 与回滚入口。

## 8. 里程碑与交付路径

### 8.1 M0：Corpus & Reverse-Engineering Baseline
目标：建立样本库、差分语料、字段词典与运行边界说明。

### 8.2 M0.5：Save Mutation Kernel Hardening
目标：在任何正式写回前，先建立完整的安全与验证闭环。

### 8.3 M1：Emblem MVP
目标：交付可写、可预览、可分享、可回滚的徽章管理器。

### 8.4 M2：AC Read-only Explorer
目标：交付 AC 浏览、依赖查看和预览来源分析，不开放正式写入。

### 8.5 M3：AC Experimental Import Lane
目标：在 5 个 AC gate 通过后，开放实验模式导入。

### 8.6 M4：Unified Library UX
目标：统一库体验，包括搜索、筛选、收藏、批量分析与删除影响分析。

### 8.7 M5：Stabilization & Release
目标：完成大样本回归、UI 打磨、兼容声明与发布策略固化。

## 9. 发布边界与降级策略

### 9.1 Stable 发布最小边界

如果 AC 的 `preview provenance` 或 `editability transition` 长期不能闭合，仍允许 stable 发布：

**Emblem Manager + AC Read-only Explorer**

即：
- 徽章写入、编辑、打包、导入导出均可稳定支持
- AC 提供浏览、来源分析、依赖风险、预览图或预览策略说明
- 不强行承诺 AC mutation

### 9.2 Experimental 功能边界

AC mutation 相关功能只能在以下条件下开放：
- 5 个 AC gate 全部通过
- 当前处于实验模式
- 已执行双备份与 dry-run
- 用户明确确认风险

### 9.3 不承诺项

当前不承诺：
- 所有 AC 都有高质量图像预览
- 所有 share AC 都能稳定转为可编辑 user AC
- AC v1 稳定分享格式立即可用
- AC mutation 在 stable 下默认开放

## 10. 验收基线摘要

### 10.1 Data Fidelity 摘要

关键要求：
- Emblem 分类与槽位映射必须 100% 正确
- AC 分类、槽位、依赖图达到计划阈值
- 写后回读与回滚必须满足稳定发布标准

### 10.2 Visual Fidelity 摘要

关键要求：
- 浏览心智接近游戏
- 预览可辨识
- 风险提示可理解
- 导入模拟可解释
- 降级状态仍可接受

### 10.3 详细验证索引

详见：
- `appendices/ac6-data-manager.validation-baseline.md`

## 11. 风险摘要

产品级风险包括：
- 存档损坏或不可恢复写入
- 依赖缺失导致资源失效
- AC 可编辑性迁移失败
- AC 预览来源长期不闭合
- 外部工具链兼容性问题

详细风险演练详见：
- `appendices/ac6-data-manager.validation-baseline.md`
- `appendices/ac6-data-manager.release-governance.md`

## 12. 关联文档

- 技术方案：`ac6-data-manager.technical-design.md`
- 验证附录：`appendices/ac6-data-manager.validation-baseline.md`
- 发布治理附录：`appendices/ac6-data-manager.release-governance.md`
- ADR：`appendices/ac6-data-manager.adr.md`

## 13. 参考依据

### 本地实现依据
- `external/ACVIEmblemCreator/gui/src/EmblemImport.cpp`
- `external/ACVIEmblemCreator/gui/src/main.cpp`
- `ACVIEmblemCreator_实现机制分析.md`

### 外部依据
- AC6 1.06 Patch Notes：<https://store.steampowered.com/news/app/1888160/view/4016716468489779702>
- WitchyBND：<https://github.com/ividyon/WitchyBND>
- ACVIEmblemCreator issue #13：<https://github.com/pawREP/ACVIEmblemCreator/issues/13>
- AC Companion About：<https://ac-companion.app/about>

## 14. 执行接力建议

### 14.1 推荐的 `$team` 命令

以下命令用于并行推进 **M0 + M0.5 + M1**，严格限定首批交付范围为 **Emblem stable lane + AC read-only baseline**：

```text
$team "按 docs/ac6-data-manager.prd.md、docs/ac6-data-manager.technical-design.md、docs/appendices/ac6-data-manager.validation-baseline.md 执行第一阶段，只做 M0、M0.5、M1。Worker1 负责 container/workspace + WitchyBND adapter + restore point + post-write readback；Worker2 负责 emblem catalog / share->user 手动选定导入 / ac6emblempkg v1；Worker3 负责 validation baseline / golden corpus / rollback / audit；Worker4 负责 GUI shell prototype（emblem library、ImportPlan、DeleteImpactPlan、audit view）；Verifier 负责按 validation-baseline 复核 gate 与验收。禁止开启任何 AC mutation；AC 仅允许 read-only baseline 与 reverse artifacts 整理。所有 mutation 必须先 dry-run，再在 shadow workspace apply，再 post-write readback；禁止 in-place overwrite；失败必须 fail-closed 并保留 incident artifact。"
```

### 14.2 推荐的后续 `$ralph` 命令

以下命令用于在 `$team` 完成后进行串行收口、整合、验收与下一阶段准入报告编制：

```text
$ralph "基于 $team 第一阶段产出做串行收口，只整合 M0、M0.5、M1，不开启 AC experimental import。任务：1）合并 container、emblem、validation、GUI 原型成果；2）修正契约不一致与遗留缺陷；3）完成 Emblem stable lane 端到端验证、回读验证、回滚演练与审计检查；4）更新 docs/ac6-data-manager.prd.md、docs/ac6-data-manager.technical-design.md 与 appendices 中的实施状态和证据索引；5）产出 M2 准入报告，明确 AC read-only explorer 的 record-map、dependency、preview-provenance 下一步工作。验收：emblem round-trip 100%，rollback 100%，audit 完整，AC 仍保持 read-only，所有 mutation 继续遵守 shadow workspace、no in-place overwrite、dry-run before apply、fail-closed。"
```

### 14.3 使用说明

1. 先执行 `$team`，只并行推进 M0、M0.5、M1。
2. `$team` 完成后，再执行 `$ralph` 做统一收口与验收。
3. 在 `preview provenance proven` 与 `editability transition proven` 未闭合前，不得把 AC mutation 拉入 stable 范围。


## 15. 实施状态与证据索引（2026-04-14）

### 15.1 当前结论

当前仓库已经完成 **M0 / M0.5 / M1 的仓库级实现收口**：
- Emblem stable lane 的核心内核、GUI 原型、验证与回滚链路已经落地。
- AC 继续保持 **read-only boundary**；未新增任何 AC apply/import/delete/repack 入口。
- 当前证据足以支持“代码实现完成 + 自动化验证通过”的仓库结论；但仍未完成真实玩家 `.sl2`、真实 WitchyBND 工具链、游戏内人工验收三类外部验证，因此发布结论仍需保守。

### 15.2 已完成范围

1. **Container / Workspace lane**
   - `src/ac6_data_manager/container_workspace/adapter.py`
   - `src/ac6_data_manager/container_workspace/workspace.py`
   - `src/ac6_data_manager/container_workspace/transaction.py`
   - 能力：shadow workspace、restore point、post-write readback、incident artifact、禁止 in-place overwrite。
2. **Emblem stable lane**
   - `src/ac6_data_manager/emblem/binary.py`
   - `src/ac6_data_manager/emblem/catalog.py`
   - `src/ac6_data_manager/emblem/importer.py`
   - `src/ac6_data_manager/emblem/package.py`
   - `src/ac6_data_manager/emblem/preview.py`
   - 能力：`USER_DATA007` / `EMBC` 二进制模型、share emblem -> user selectable import、`ac6emblempkg` v1、预览 fail-soft 占位。
3. **Validation / Audit / Rollback lane**
   - `src/ac6_data_manager/validation/audit.py`
   - `src/ac6_data_manager/validation/baseline.py`
   - `src/ac6_data_manager/validation/corpus.py`
   - `src/ac6_data_manager/validation/rollback.py`
   - 能力：audit JSONL、incident bundle index、golden corpus manifest、restore point rollback、M2 readiness 文档。
4. **GUI shell prototype**
   - `src/ac6_data_manager/gui/dto.py`
   - `src/ac6_data_manager/gui/models.py`
   - `src/ac6_data_manager/gui/dialogs.py`
   - `src/ac6_data_manager/gui/main_window.py`
   - `src/ac6_data_manager/app.py`
   - 技术栈：PyQt5。
   - 能力：emblem library、ImportPlan dialog、DeleteImpactPlan dialog、audit view。

### 15.3 自动化证据

- 全量测试命令：`QT_QPA_PLATFORM=offscreen PYTHONPATH=src pytest tests -q`
- 全量结果：`27 passed`
- 关键证据文件：
  - `artifacts/roundtrip-reports/emblem-stable-lane-automated.md`
  - `artifacts/verification/phase1/pytest-summary.json`
  - `docs/appendices/ac6-data-manager.m0-m1-verifier-report.md`
  - `docs/appendices/ac6-data-manager.m2-readiness.md`

### 15.4 发布前仍需补齐的外部验证

以下事项未在本轮仓库自动化中完成，仍是 stable 发布前的必要外部验证：
1. 使用真实 WitchyBND 与真实 `.sl2` 做 end-to-end apply / rollback 演练。
2. 在游戏内人工确认 emblem 导入后“可见、可编辑、可保存、预览正确”。
3. 建立 `docs/reverse/` 与 AC read-only explorer 所需的 reverse artifacts，再推进 M2 证据闭环。

### 15.5 当前产品级结论

- **可以对外宣称**：M0 / M0.5 / M1 的仓库实现已完成，Emblem stable lane 具备代码与自动化验证基础。
- **暂不能对外宣称**：真实存档稳定发布已完成、AC mutation 可用、AC experimental import 已解锁。
