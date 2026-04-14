# Validation Lane Artifacts

本目录仅承载 validation lane 基线工件，不承载任何 AC mutation 输出。

## 子目录
- `golden-corpus/`：golden corpus 清单、样本命名约定与校验基线。
- `audit/`：审计日志字段、blocked/apply/rollback 事件约定。
- `incidents/`：fail-closed incident bundle 目录约定。
- `roundtrip-reports/`：round-trip、readback 与回滚演练报告占位目录。

## 约束
- 不在此目录提交真实玩家存档。
- 不在此目录提交任何包含敏感路径或凭据的原始日志。
- AC 仅允许 read-only baseline 与 reverse artifacts 整理；不得提交 AC mutation 样本。
