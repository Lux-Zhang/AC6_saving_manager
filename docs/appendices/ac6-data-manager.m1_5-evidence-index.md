# AC6 Data Manager M1.5 Evidence Index

## 1. 自动化证据位点

| 证据 | 路径 | 生产方式 | 当前状态 | verdict 输入 |
|---|---|---|---|---|
| release content manifest | `artifacts/release/content-manifest.json` | `python scripts/build_windows_portable.py ...` | 已实现 | `evidence_complete` |
| evidence manifest baseline | `artifacts/release/evidence-manifest.json` | `python scripts/build_windows_portable.py ...` | 已实现 | baseline verdict = `release-pending` |
| third-party report | `artifacts/preflight/third-party-report.json` | `python scripts/build_windows_portable.py ...` | 已实现 | `preflight_ok` / sidecar gate |
| preflight report | `artifacts/preflight/preflight-report.json` | `python scripts/build_windows_portable.py ...` 或 `python scripts/run_preflight_check.py ...` | 已实现 | `preflight_ok` |
| preflight text | `artifacts/preflight/preflight-report.txt` | 同上 | 已实现 | 人工复核摘要 |
| real-entry proof | `artifacts/preflight/real-entry-proof.json` | `emit_provider_proof` / `--provider-proof-out` | 已实现 | `real_entry_ok` |
| smoke provider proof | `artifacts/live_acceptance/smoke-provider-proof.json` | `powershell -File scripts/smoke_test_portable.ps1 ...` | 已实现 | 仓内 smoke baseline |
| smoke preflight | `artifacts/live_acceptance/smoke-preflight.json`、`smoke-preflight.txt` | `powershell -File scripts/smoke_test_portable.ps1 ...` | 已实现 | 仓内 smoke baseline |
| manual checklist template | `artifacts/live_acceptance/checklist-manual.md` | `powershell -File scripts/live_acceptance_capture.ps1 ...` | 已实现 | `evidence_complete` |
| publish audit | `artifacts/live_acceptance/publish-audit-*.json` | `src/ac6_data_manager/release/publish.py::write_publish_artifacts` | 已实现 | `publish_ok` |
| readback artifact | `artifacts/live_acceptance/readback-*.json` | `write_publish_artifacts` | 已实现 | `publish_ok` / `evidence_complete` |
| rollback artifact | `artifacts/live_acceptance/rollback-*.json` | `write_rollback_artifact` | 已实现 | `rollback_ok` |
| incident artifact | `artifacts/live_acceptance/incident/incident-*.json` | `write_publish_artifacts(..., incident_root=...)` | 已实现 | 失败诊断 |

## 2. 人工证据位点

| 证据 | 推荐路径 | 责任人 / 阶段 | 用途 |
|---|---|---|---|
| Huorong matrix | `artifacts/huorong/huorong-matrix.json` 或 `.md` | ZPX-GE77 live acceptance | `huorong_matrix` |
| Huorong 弹窗截图 | `artifacts/huorong/screenshots/` | ZPX-GE77 live acceptance | 支撑 `pass` / `manual-allow` / `blocked` 分类 |
| Huorong 观察记录 | `artifacts/huorong/notes.md` | ZPX-GE77 live acceptance | 补充说明 |
| first launch / second launch 截图 | `artifacts/live_acceptance/screenshots/` | ZPX-GE77 live acceptance | 证明首启与二次启动 |
| 游戏内复核记录 | `artifacts/live_acceptance/game-verify.md` 或截图 | ZPX-GE77 live acceptance | `game_verify_ok` |
| 最终 live acceptance 记录 | `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md` | ZPX-GE77 live acceptance | 最终 verdict 归档 |

## 3. Verdict 输入映射

`src/ac6_data_manager/release/evidence.py::derive_release_verdict(...)` 依赖以下输入：

| 输入 | 证据来源 | 判定方式 |
|---|---|---|
| `huorong_matrix` | `artifacts/huorong/huorong-matrix.*` | 任一步骤 `blocked` 直接 `fail`；存在 `manual-allow` 且其余条件满足时为 `conditional-pass` |
| `preflight_ok` | `artifacts/preflight/preflight-report.json` | `overall_status == "pass"` |
| `real_entry_ok` | `artifacts/preflight/real-entry-proof.json` | `provider_kind == "service_bundle"` 且 `is_demo == false` |
| `publish_ok` | `publish-audit-*.json` + `readback-*.json` | apply 成功且 readback 证据齐全 |
| `rollback_ok` | `rollback-*.json` | rollback 成功且输出为 fresh destination |
| `game_verify_ok` | 游戏内复核记录 | `visible/editable/saveable` 全部完成 |
| `evidence_complete` | 本索引所有强制证据项 | 自动化 + 人工证据均已落盘 |

## 4. 强制证据清单

以下证据缺任一项，都不得宣称 `live-pass`：

1. `artifacts/release/content-manifest.json`
2. `artifacts/release/evidence-manifest.json`
3. `artifacts/preflight/third-party-report.json`
4. `artifacts/preflight/preflight-report.json`
5. `artifacts/preflight/real-entry-proof.json`
6. `artifacts/live_acceptance/checklist-manual.md`
7. `artifacts/live_acceptance/publish-audit-*.json`
8. `artifacts/live_acceptance/readback-*.json`
9. `artifacts/live_acceptance/rollback-*.json`
10. `artifacts/huorong/huorong-matrix.*`
11. 游戏内 `visible/editable/saveable` 记录
12. `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`

## 5. 当前缺口

- `artifacts/huorong/` 的真实矩阵与截图仍待实机执行
- 游戏内 `visible/editable/saveable` 复核仍待实机执行
- `ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md` 仍待回填真实结果

## 6. 关联文档

- `docs/appendices/ac6-data-manager.m1_5-live-acceptance-checklist.md`
- `docs/appendices/ac6-data-manager.m1_5-release-runbook.md`
- `docs/appendices/ac6-data-manager.m1_5-huorong-risk-notes.md`
- `docs/appendices/ac6-data-manager.m1_5-release-verdict-truth-table.md`
