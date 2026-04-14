# AC6 Data Manager M1.5 Live Acceptance Checklist

- 状态：Execution Ready / Manual Evidence Pending
- 验收机：`ZPX-GE77`
- 适用 release root：`artifacts/release/AC6 saving manager/`
- 关联脚本：`scripts/live_acceptance_capture.ps1`、`scripts/smoke_test_portable.ps1`、`scripts/run_preflight_check.py`
- 关联附录：`ac6-data-manager.m1_5-release-runbook.md`、`ac6-data-manager.m1_5-huorong-risk-notes.md`、`ac6-data-manager.m1_5-evidence-index.md`、`ac6-data-manager.m1_5-release-verdict-truth-table.md`

## 1. 执行前登记

| 字段 | 填写 |
|---|---|
| release label |  |
| portable zip SHA256 |  |
| operator |  |
| 执行日期 |  |
| Huorong 版本 |  |
| source save 路径 |  |
| apply fresh destination |  |
| rollback fresh destination |  |
| 备注 |  |

## 2. 固定前提

- [ ] 当前产物来自显式 5-worker 执行车道；若实际 worker pane 少于 5，本次产物直接视为无效
- [ ] AC mutation 仍然禁止；五个 AC gate 保持 locked
- [ ] 所有 save write 继续满足：`dry-run before apply`、`shadow workspace`、`post-write readback`、`no in-place overwrite`、`fresh-destination-only`、`fail-closed`
- [ ] 验收目录是新的空目录，未复用历史输出
- [ ] `third_party/WitchyBND/` 与 `third_party/third_party_manifest.json` 均来自当前 release root，不依赖系统 PATH

## 3. 预检查

- [ ] `ZPX-GE77` / Windows 10 25H2 x64
- [ ] Huorong enabled
- [ ] offline path confirmed
- [ ] release root = `artifacts/release/AC6 saving manager/`
- [ ] `AC6 saving manager.exe` 存在
- [ ] `third_party/WitchyBND/WitchyBND.exe` 存在
- [ ] `third_party/third_party_manifest.json` 存在
- [ ] `docs/LIVE_ACCEPTANCE_CHECKLIST.txt`、`docs/KNOWN_LIMITATIONS.txt`、`docs/QUICK_START.txt` 存在
- [ ] 已执行 `powershell -File scripts/live_acceptance_capture.ps1 -ReleaseRoot "<release_root>" -OutputDir artifacts/live_acceptance`
- [ ] `artifacts/live_acceptance/checklist-manual.md` 已生成

## 4. Build / release 基线证据

| 检查项 | 证据路径 | 通过条件 |
|---|---|---|
| release content manifest | `artifacts/release/content-manifest.json` | 文件存在，覆盖 release 目录全部文件 |
| evidence manifest baseline | `artifacts/release/evidence-manifest.json` | `verdict=release-pending`，且 notes 明确需补 ZPX-GE77 实机证据 |
| third-party gate | `artifacts/preflight/third-party-report.json` | `success=true`，version label 与本次 release label 一致 |
| provider proof | `artifacts/preflight/real-entry-proof.json` | `provider_kind=service_bundle` 且 `is_demo=false` |
| preflight JSON/TXT | `artifacts/preflight/preflight-report.json`、`artifacts/preflight/preflight-report.txt` | `overall_status=pass`，关键 checks 全部为 `pass` |
| smoke baseline（可选但建议） | `artifacts/live_acceptance/smoke-provider-proof.json`、`smoke-preflight.json`、`smoke-preflight.txt` | 作为 live 验收前的仓内回归基线 |

## 5. 首启与 proof

- [ ] 解压 portable zip 到新的空目录
- [ ] 首次启动命令：`AC6 saving manager.exe --provider-proof-json <path>`
- [ ] provider proof 记录 `provider_kind=service_bundle`
- [ ] provider proof 记录 `is_demo=false`
- [ ] 首启截图已保存
- [ ] Huorong 对首启的结果已记录为 `pass` / `manual-allow` / `blocked`

## 6. Preflight 与 sidecar gate

执行命令：

```bash
python scripts/run_preflight_check.py \
  --release-root artifacts/release/AC6\ saving\ manager \
  --expected-version-label <label> \
  --json-out artifacts/preflight/preflight-report.json \
  --text-out artifacts/preflight/preflight-report.txt \
  --provider-proof-out artifacts/preflight/real-entry-proof.json
```

- [ ] `overall_status=pass`
- [ ] `release-layout=pass`
- [ ] `third-party-manifest=pass`
- [ ] `runtime-entry=pass`
- [ ] `publish-contract=pass`
- [ ] `offline-readiness=pass`
- [ ] Huorong 对 preflight / sidecar gate 的结果已记录

## 7. Stable lane 主路径

- [ ] 浏览 share emblem
- [ ] 生成 ImportPlan
- [ ] dry-run
- [ ] apply 到 fresh destination
- [ ] `publish-audit-*.json` 已保存
- [ ] `readback-*.json` 已保存
- [ ] 若出现 incident，则 `incident/incident-*.json` 已保存
- [ ] 未发生原地覆盖 source save
- [ ] Huorong 对 dry-run / apply 的结果已记录

## 8. Rollback Drill

- [ ] rollback 到 fresh destination
- [ ] `rollback-*.json` 已保存
- [ ] rollback 输出路径与 source save / backup path 不同
- [ ] Huorong 对 rollback 的结果已记录

## 9. 二次启动与游戏内复核

- [ ] second launch 完成
- [ ] 游戏内 `visible` 通过
- [ ] 游戏内 `editable` 通过
- [ ] 游戏内 `saveable` 通过
- [ ] 游戏内截图或操作说明已保存
- [ ] Huorong 对 second launch 的结果已记录

## 10. Huorong Matrix

| 步骤 | 结果（pass/manual-allow/blocked） | 证据 | 备注 |
|---|---|---|---|
| unzip |  |  |  |
| first launch |  |  |  |
| preflight |  |  |  |
| sidecar |  |  |  |
| dry-run |  |  |  |
| apply |  |  |  |
| rollback |  |  |  |
| second launch |  |  |  |

## 11. Release Verdict Gate

- [ ] Huorong Matrix 中不存在 `blocked`
- [ ] preflight pass
- [ ] real entry proof = `service_bundle`
- [ ] publish / readback / rollback 证据齐全
- [ ] 游戏内 `visible/editable/saveable` 全部完成
- [ ] `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md` 已回填
- [ ] 已按 `ac6-data-manager.m1_5-release-verdict-truth-table.md` 计算最终 verdict

## 12. 必须归档的最终证据

- `artifacts/release/content-manifest.json`
- `artifacts/release/evidence-manifest.json`
- `artifacts/preflight/third-party-report.json`
- `artifacts/preflight/preflight-report.json`
- `artifacts/preflight/preflight-report.txt`
- `artifacts/preflight/real-entry-proof.json`
- `artifacts/live_acceptance/checklist-manual.md`
- `artifacts/live_acceptance/publish-audit-*.json`
- `artifacts/live_acceptance/readback-*.json`
- `artifacts/live_acceptance/rollback-*.json`
- `artifacts/live_acceptance/incident/incident-*.json`（若存在）
- `artifacts/huorong/` 下的矩阵、截图、观察记录
- `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`
