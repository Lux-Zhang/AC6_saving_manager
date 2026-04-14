# AC6 Data Manager Validation Baseline

- 状态：Approved Planning Baseline
- 版本：v1.0-baseline
- 最后更新：2026-04-14
- 关联主文档：
  - `../ac6-data-manager.prd.md`
  - `../ac6-data-manager.technical-design.md`

## 1. 文档边界

本文档承载量化 gate、验证矩阵、验收表、故障注入与审计要求，是验证与发布评审的判定依据。

## 2. 5 个 AC Gates 量化定义

### 2.1 Gate 1：record map proven

#### 样本范围
- 至少 30 个样本存档
- 覆盖空槽、少量 AC、中量 AC、接近满槽、大量 decals、多分享 AC、用户可编辑 AC
- 样本来源至少包括原始玩家样本、控制变量样本、差分实验样本

#### 证据产物
- `docs/reverse/ac-record-map.md`
- `artifacts/ac-record-diff-corpus/*.json`
- 字段级差分表
- 槽位映射表
- 未知字段列表

#### 通过条件
- 100% 识别 AC 记录边界与槽位归属
- 关键字段分类准确率 >= 95%
- 未知字段字节占比 <= 10%
- 未知字段不影响只读浏览正确性

#### 失败条件
- 任一样本无法稳定定位记录边界
- 任一样本槽位归属错误
- 关键字段分类准确率 < 95%

#### 解锁条件
- 允许继续依赖图与预览来源分析
- 不允许 AC 写入

### 2.2 Gate 2：dependency graph proven

#### 样本范围
- 从 30 个样本中选 15 个高复杂样本
- 每个样本至少覆盖 emblem / image-decal / 预览相关依赖候选
- 至少 10 组删除或替换差分实验

#### 证据产物
- `docs/reverse/ac-dependency-graph.md`
- `artifacts/ac-dependency-traces/*.json`
- dependency graph schema
- 删除影响报告样本

#### 通过条件
- 依赖解析 precision >= 95%
- 依赖解析 recall >= 95%
- 删除影响模拟与人工游戏验证一致率 >= 90%
- 可区分强依赖与可降级依赖

#### 失败条件
- 高复杂样本存在未解释核心依赖缺失并导致游戏内显示错误
- 删除影响预测错漏率 > 10%

#### 解锁条件
- 允许 AC ImportPlan / DeleteImpactPlan dry-run 原型
- 不允许正式 AC 写入

### 2.3 Gate 3：editability transition proven

#### 样本范围
- 至少 12 组 share AC -> user AC 控制变量实验
- 覆盖纯 AC、重 decal AC、带 emblem AC、接近满槽、新增路径、替换路径

#### 证据产物
- `docs/reverse/ac-editability-transition.md`
- `artifacts/ac-transition-experiments/*.md`
- 槽位重写规则
- 来源状态转换规则
- 迁移字段清单

#### 通过条件
- 至少 11/12 组实验可进入编辑界面且保存成功
- 关键依赖保留率 >= 95%
- 无读档崩溃
- 无明显资源错位

#### 失败条件
- 任一实验导致不可恢复损档或无法读档
- 可编辑成功率 < 90%
- 关键依赖保留率 < 95%

#### 解锁条件
- 允许 experimental import
- 不允许 stable import

### 2.4 Gate 4：preview provenance proven

#### 样本范围
- 至少 20 个 AC 样本
- 覆盖有预览、缺预览、修改后预览变化、分享/用户两类来源
- 至少 8 组“字段变动 -> 预览变化”差分实验

#### 证据产物
- `docs/reverse/ac-preview-provenance.md`
- `artifacts/ac-preview-traces/*.json`
- 提取路径证明或 fallback renderer 判定书

#### 通过条件
- AC 预览可被归入以下三类之一：可直接提取 / 可接受成本 fallback / 只能占位
- 预览策略分派正确率 >= 90%
- 若为提取路径：提取成功率 >= 95%
- 若为 fallback：生成成功率 >= 90%，平均单项 <= 2 秒

#### 失败条件
- 预览来源仍无法归因
- 预览策略分派正确率 < 90%

#### 解锁条件
- 允许冻结 AC 浏览预览布局与缓存策略
- 若仅能占位，不阻塞只读发布，但阻塞高密度图像预览承诺

### 2.5 Gate 5：round-trip corpus proven

#### 样本范围
- Emblem lane：至少 20 个写入回读样本
- AC experimental lane：至少 12 个实验写入样本
- 覆盖新增、替换、删除回滚、包导入导出、异常中断恢复

#### 证据产物
- `artifacts/roundtrip-reports/*.md`
- golden corpus hash 清单
- 回读一致性报告
- 回滚演练记录
- 异常注入日志

#### 通过条件
- Emblem round-trip 成功率 100%
- AC experimental round-trip 成功率 >= 90%
- AC experimental 0 个不可恢复损档
- 回滚成功率 100%
- 异常中断场景下原始存档可恢复率 100%

#### 失败条件
- 任一不可恢复损档
- 任一回滚失败
- Emblem round-trip 非 100%

#### 解锁条件
- Emblem lane 可 stable 发布
- AC lane 可保持 experimental 发布
- AC stable 仍需满足治理附录中的升级规则

## 3. Milestone 验证矩阵

### 3.1 M0：Corpus & Reverse-Engineering Baseline
- 单元：样本索引、标签系统、元数据读取
- 集成：解包 -> workspace -> 扫描成功率 100%
- GUI E2E：样本浏览器显示基础元信息
- 游戏内人工验收：无
- 故障注入：缺文件、工具缺失、权限不足，要求 fail-closed

### 3.2 M0.5：Save Mutation Kernel Hardening
- 单元：restore point 命名、checksum、manifest、audit schema、invariant rule
- 集成：shadow workspace 回包后回读、reference validator 对断链报警
- GUI E2E：dry-run -> ImportPlan -> 阻止不合格 Apply
- 游戏内人工验收：测试存档验证“失败后原档未变”
- 故障注入：回包中断、写后校验失败、工具版本不匹配，要求自动阻断与回滚成功率 100%

### 3.3 M1：Emblem MVP
- 单元：emblem parser、slot allocator、package v1 manifest、preview extractor
- 集成：手动选定分享徽章导入 -> 回读一致；徽章包导入导出 -> 他档导入成功
- GUI E2E：浏览、筛选、预览、导入模拟、Apply、回滚、审计
- 游戏内人工验收：可见、可编辑、可保存、预览正确
- 故障注入：槽位满、冲突、包校验失败、回读失败

### 3.4 M2：AC Read-only Explorer
- 单元：AC classifier、dependency candidate parser、preview strategy selector
- 集成：扫描 30 个样本生成 catalog、dependency traces、preview traces
- GUI E2E：AC 列表、详情、依赖图、风险标签
- 游戏内人工验收：工具结果与游戏目录对照一致
- 故障注入：未知字段超阈值时退回只读警告模式

### 3.5 M3：AC Experimental Import Lane
- 单元：transition rule、DeleteImpactPlan、experimental manifest、conflict detector
- 集成：12 组 share AC -> user AC 导入实验；experimental v0 包闭环
- GUI E2E：选定 -> dry-run -> 风险确认 -> experimental Apply -> 审计 -> 回滚
- 游戏内人工验收：可编辑、关键依赖不丢、读档不异常
- 故障注入：人工打断、伪造断链、伪造 preview miss

### 3.6 M4：Unified Library UX
- 单元：search index、favorites、tags、recent imports、sorting
- 集成：Emblem/AC catalog 在统一 UX 中共存但不混淆语义
- GUI E2E：卡片/列表切换、组合筛选、批量导入模拟、删除影响分析
- 游戏内人工验收：复用 M1/M3 基线
- 故障注入：缓存损坏、缩略图失效，退回占位模式

### 3.7 M5：Stabilization & Release
- 单元：兼容矩阵、设置迁移、日志脱敏规则
- 集成：golden corpus 全量回归、连续版本升级下包兼容性验证
- GUI E2E：首次启动 -> 打开存档 -> 备份 -> 导入 -> 回滚 -> 查看审计 -> 退出恢复
- 游戏内人工验收：按发布边界执行完整 checklist
- 故障注入：磁盘空间不足、权限错误、损坏包、未知补丁版本

## 4. Data Fidelity 验收表

| 验收项 | 指标 | 通过条件 | 阻塞条件 |
|---|---|---|---|
| 记录分类正确 | 分类准确率 | Emblem 100%；AC >= 95% | 低于阈值 |
| 槽位归属正确 | 槽位映射准确率 | Emblem 100%；AC >= 95% | 误归属导致写入风险 |
| 依赖图正确 | precision / recall | 均 >= 95% | 任一 < 95% |
| 删除影响分析正确 | 与人工结果一致率 | >= 90% | 低于 90% |
| 写后回读一致 | 回读一致率 | stable 100%；experimental >= 90% 且无不可恢复损档 | 任一不可恢复损档 |
| 回滚可恢复 | 回滚成功率 | 100% | 任一失败 |
| 审计完整 | 事件记录完整率 | 100% | 丢关键事件 |
| 包兼容声明正确 | 导入前报告准确率 | >= 95% | 高风险误判 |

## 5. Visual Fidelity 验收表

| 验收项 | 指标 | 通过条件 | 阻塞条件 |
|---|---|---|---|
| 浏览布局接近游戏心智 | 任务完成测试 | 5 名测试者中至少 4 名可在 30 秒内找到目标资产并打开详情 | 低于阈值 |
| 预览可辨识度 | 可辨识率 | Emblem >= 95%；AC 已支持样本 >= 90% | 低于阈值 |
| 风险提示清晰 | 用户理解测试 | 5 名测试者中至少 4 名能说明是否可回滚、是否 experimental | 低于阈值 |
| 导入模拟可解释 | 解释正确率 | >= 80% 测试者能说出新增/替换/风险点 | 低于阈值 |
| 卡片/列表效率 | 操作时间 | 常见任务在两种模式下均可于 45 秒内完成 | 低于阈值 |
| 降级状态可接受 | 可用性评分 | 占位/元信息模式平均满意度 >= 3.5/5 | 低于阈值 |

## 6. 故障注入与回滚演练基线

### 6.1 Scenario A：回包成功但游戏内损档或读档异常
- 侦测点：readback mismatch、invariant checker 报错、游戏内读档失败
- 阻断策略：阻止覆盖原档；冻结相关 mutation rule；生成 incident artifact
- 回滚策略：恢复 restore point；标记规则 quarantined

### 6.2 Scenario B：依赖图判断错误导致删除后引用失效
- 侦测点：DeleteImpactPlan 与人工结果不一致；删除后游戏内贴花/徽章缺失
- 阻断策略：低 confidence 只允许 dry-run；删除动作二次确认；展示置信度
- 回滚策略：恢复删除前 restore point；将依赖模式写入 blocker rule

### 6.3 Scenario C：实验导入导致资源错位或不可编辑
- 侦测点：editability test 失败；游戏内可打开但保存失败；预览与实际错位
- 阻断策略：experimental Apply 前双备份；transition confidence 不足则仅允许 dry-run
- 回滚策略：一键回滚；将失败样本纳入语料并停止自动推荐相同模式

## 7. Observability / Audit 要求

每次 Apply / Rollback / Blocked action 至少记录：
- 时间
- save hash
- workspace id
- toolchain version
- rule set version
- plan summary
- result status

高风险失败必须自动产出 incident bundle，供复现与后续治理使用。


## 8. 当前实现证据索引（2026-04-14）

### 8.1 自动化回归结果

- 命令：`QT_QPA_PLATFORM=offscreen PYTHONPATH=src pytest tests -q`
- 结果：`27 passed`
- 机器可读摘要：`../../artifacts/verification/phase1/pytest-summary.json`

### 8.2 M0 / M0.5 / M1 当前状态

| Milestone | 当前仓库结论 | 已有证据 | 仍缺项 |
|---|---|---|---|
| `M0` | `PARTIAL` | 已有 workspace / audit / golden corpus helper 等基础设施。 | `docs/reverse/`、样本索引、真实扫描 corpus 仍缺。 |
| `M0.5` | `REPO-PASS / LIVE-PENDING` | shadow workspace、restore point、readback、incident、rollback、audit 已实现并有自动化测试。 | 真实 WitchyBND + 真实 `.sl2` + 游戏外部验证待补。 |
| `M1` | `REPO-PASS / RELEASE-PENDING` | emblem binary、share->user import、package v1、preview、PyQt5 GUI、audit/rollback 已实现并有自动化测试。 | 游戏内“可见/可编辑/可保存/预览正确”与真实存档演练待补。 |

### 8.3 关键证据文件

- `tests/test_container_workspace.py`
- `tests/test_emblem_binary.py`
- `tests/test_emblem_import.py`
- `tests/test_emblem_package.py`
- `tests/test_emblem_preview.py`
- `tests/test_gui_models.py`
- `tests/test_gui_windows.py`
- `tests/test_validation_audit.py`
- `tests/test_validation_corpus.py`
- `tests/test_validation_rollback.py`
- `artifacts/roundtrip-reports/emblem-stable-lane-automated.md`
- `docs/appendices/ac6-data-manager.m0-m1-verifier-report.md`
- `docs/appendices/ac6-data-manager.m2-readiness.md`

### 8.4 当前 gate 结论

- Emblem stable lane：仓库级实现已闭合，自动化回归通过。
- AC lane：5 个 gate 继续保持锁定；当前仅允许 read-only baseline 与 reverse artifacts 整理。
- 只有在 `docs/reverse/` 与 AC gate 产物补齐后，才能重新发起 M2/M3 评审。
