# AC6 Data Manager M1.5 ZPX-GE77 Live Acceptance Record

## 1. 执行元数据

| 字段 | 值 |
|---|---|
| 机器 | `ZPX-GE77` |
| 系统 | Windows 10 25H2 x64 |
| 安全软件 | Huorong enabled |
| 当前状态 | Pending execution |
| operator |  |
| 执行日期 |  |
| release label |  |
| portable zip SHA256 |  |

## 2. 仓内准备状态

- 仓内实现与自动化验证：完成
- Windows portable build / preflight / evidence 脚本：完成
- live acceptance checklist / runbook / Huorong matrix / evidence index / verdict 真值表：完成

## 3. 实机执行记录

| 阶段 | 结果 | 证据 | 备注 |
|---|---|---|---|
| unzip | Pending |  |  |
| first launch | Pending |  |  |
| preflight | Pending |  |  |
| sidecar gate | Pending |  |  |
| dry-run | Pending |  |  |
| apply | Pending |  |  |
| readback | Pending |  |  |
| rollback | Pending |  |  |
| second launch | Pending |  |  |
| visible | Pending |  |  |
| editable | Pending |  |  |
| saveable | Pending |  |  |

## 4. Huorong 结果映射

| 步骤 | `pass` / `manual-allow` / `blocked` | 证据 | 备注 |
|---|---|---|---|
| unzip |  |  |  |
| first launch |  |  |  |
| preflight |  |  |  |
| sidecar |  |  |  |
| dry-run |  |  |  |
| apply |  |  |  |
| rollback |  |  |  |
| second launch |  |  |  |

## 5. 强制证据清单

- [ ] `artifacts/release/content-manifest.json`
- [ ] `artifacts/release/evidence-manifest.json`
- [ ] `artifacts/preflight/third-party-report.json`
- [ ] `artifacts/preflight/preflight-report.json`
- [ ] `artifacts/preflight/real-entry-proof.json`
- [ ] `artifacts/live_acceptance/checklist-manual.md`
- [ ] `artifacts/live_acceptance/publish-audit-*.json`
- [ ] `artifacts/live_acceptance/readback-*.json`
- [ ] `artifacts/live_acceptance/rollback-*.json`
- [ ] `artifacts/huorong/huorong-matrix.*`
- [ ] 游戏内 `visible/editable/saveable` 证据

## 6. 当前 verdict

- 当前判定：`release-ready / live-pending`
- 原因：当前会话未直接产生 `ZPX-GE77 + Huorong enabled` 真机证据，不能提升到 `live-pass`
- 计算依据：`docs/appendices/ac6-data-manager.m1_5-release-verdict-truth-table.md`

## 7. 后续动作

1. 在 `ZPX-GE77` 上按 checklist 与 runbook 执行真实 live acceptance。
2. 回填本文件中的阶段结果、Huorong 结果与证据路径。
3. 依据真值表把最终 verdict 更新为 `live-pass`、`conditional-pass`、`release-pending` 或 `fail`。
