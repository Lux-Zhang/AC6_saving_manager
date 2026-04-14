# Round-trip Reports Baseline

本目录保留 round-trip、post-write readback 与 rollback rehearsal 的报告模板。

## 必填摘要
- 样本标识
- 操作类型（新增 / 替换 / 删除回滚 / 包导入导出）
- readback 结果
- rollback 结果
- incident bundle 引用（若有）

## 当前阶段约束
- M0 / M0.5 / M1 仅要求 Emblem lane 与 validation lane 的报告模板。
- AC mutation 报告不得在未通过五个 AC gate 前进入 stable 结论。
