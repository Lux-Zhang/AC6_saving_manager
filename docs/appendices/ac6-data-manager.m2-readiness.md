# AC6 Data Manager M2 Readiness

## 1. 文档目的
本附录用于承接 M0.5 / M1 收口后的下一阶段准入判断，只覆盖 M2 的 AC read-only explorer。
当前结论：M2 仅可作为 read-only lane 继续推进，不得借此开启任何 AC mutation、experimental import 或隐式写回。

## 2. 当前准入结论
- `record-map`: 未达成 proven。当前仅允许继续整理逆向样本、字段对照、偏移变化模式与命名约定。
- `dependency`: 未达成 proven。当前仅允许建立只读依赖观察模型、引用关系候选图与人工复核流程。
- `preview-provenance`: 未达成 proven。当前仅允许输出来源标注、占位图、候选提取链路，不得伪装为稳定预览。
- `editability transition` 与 `round-trip corpus`: 本附录不申请解锁，继续维持关闭状态。

## 3. 下一步工作分解

### 3.1 record-map
目标：把 AC 记录从“疑似结构”推进到“可重复验证的只读映射”。

下一步：
1. 固化 `artifacts/ac-record-diff-corpus/` 样本命名与采样批次规则。
2. 为每一类 AC 变更建立最小差分样本，并记录字段偏移、长度、类型假设、置信度。
3. 输出 machine-readable record map 草案，支持字段级证据回链。
4. 对同一字段在多样本、多版本中的稳定性做一致性检查。

准入门槛：
- 同类记录在 golden corpus 中可稳定复现。
- 字段定义具备证据回链与人工复核结论。
- 未解决冲突项明确列入 blocker list。

### 3.2 dependency
目标：建立 AC 只读依赖观察能力，说明部件、贴花、预设、预览之间的引用边界。

下一步：
1. 整理候选依赖边类型、方向与证据来源。
2. 输出 dependency graph 草案，并标注 unknown / inferred / confirmed 三种置信度层级。
3. 对删除、替换、浏览三个 read-only 场景给出依赖可见性要求。
4. 准备人工复核模板，记录误判样本与 blocker rule。

准入门槛：
- 依赖边类型在样本集中可重复观察。
- 候选图中的 inferred 边可追溯到具体样本。
- GUI read-only explorer 可明确显示置信度与证据来源。

### 3.3 preview-provenance
目标：在未证明真实预览来源前，保持“来源透明、体验降级可接受”的只读策略。

下一步：
1. 为每一种预览输出标记 provenance：direct / derived / placeholder。
2. 建立预览生成链路记录，明确输入、工具链、降级原因与时间戳。
3. 收集社区或样本反馈，区分“看起来像”与“已证明正确”的预览路径。
4. 为 GUI 详情面板补充 provenance 展示字段与风险提示。

准入门槛：
- 任一预览结果都能回溯到生成链路。
- derived / placeholder 结果在 GUI 中显式标记。
- 未闭合来源的能力不得进入 stable 事实描述。

## 4. M2 开发约束
- 保持 AC lane 全程只读。
- 允许新增 reverse artifacts、分析脚本、只读 GUI 数据视图。
- 禁止新增任何 AC apply、repack、save overwrite、share->user mutation 入口。
- 所有新证据必须进入 gate 产物目录，并可被 validation / audit lane 索引。

## 5. 建议的收口产物
- `record-map` 草案与字段证据索引。
- `dependency` 候选图与 blocker 清单。
- `preview-provenance` 决策表与 GUI 展示样例。
- 新一轮 gate review 结论，明确是否仍保持 read-only。
