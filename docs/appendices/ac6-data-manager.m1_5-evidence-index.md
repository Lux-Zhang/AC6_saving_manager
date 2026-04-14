# AC6 Data Manager M1.5 Evidence Index

## 1. 目的
统一索引 M1.5 build、preflight、portable release、live acceptance、Huorong、publish 与 rollback 证据。

## 2. 最小证据清单
| 类别 | 建议路径 | 必需 |
|---|---|---|
| build log | `artifacts/release/build-*.log` | 是 |
| release content manifest | `artifacts/release/content-manifest-*.json` | 是 |
| third-party gate report | `artifacts/preflight/third-party-*.json` | 是 |
| preflight report | `artifacts/preflight/preflight-*.json` | 是 |
| real-entry proof | `artifacts/preflight/real-entry-*.json` | 是 |
| smoke report | `artifacts/live_acceptance/smoke-*.json` | 是 |
| Huorong matrix | `artifacts/huorong/matrix-*.md` | 是 |
| publish audit | `artifacts/live_acceptance/publish-audit-*.jsonl` | 是 |
| readback evidence | `artifacts/live_acceptance/readback-*.json` | 是 |
| rollback report | `artifacts/live_acceptance/rollback-*.json` | 是 |
| checklist capture | `artifacts/live_acceptance/checklist-*.md` | 是 |
| incident artifact | `artifacts/incident/*.json` | 条件必需 |
| game verify note | `artifacts/live_acceptance/game-verify-*.md` | 是 |

## 3. 证据包 manifest
最终 evidence pack 至少包含：
- build metadata
- release content manifest
- third-party gate result
- preflight result
- real-entry proof
- Huorong matrix
- publish audit
- readback evidence
- rollback evidence
- final checklist
- release verdict

## 4. 证据索引维护规则
- 每完成一个关键阶段，立即补索引。
- 若阶段失败，保留失败证据，不得仅保留最终成功版本。
- 所有时间戳使用 ISO8601 或 `YYYYMMDD-HHMMSS`。
