# AC6 Data Manager M1.5 Release Verdict Truth Table

本文直接对应 `src/ac6_data_manager/release/evidence.py::derive_release_verdict(...)` 的判定顺序，用于把 live acceptance 证据转换成最终 release verdict。

## 1. 输入定义

| 输入 | 含义 | 主要证据 |
|---|---|---|
| `huorong_matrix` | Huorong 对 unzip / first launch / preflight / sidecar / dry-run / apply / rollback / second launch 的记录 | `artifacts/huorong/huorong-matrix.*` |
| `preflight_ok` | preflight 是否整体通过 | `artifacts/preflight/preflight-report.json` |
| `real_entry_ok` | 默认入口是否确认为真实 service bundle 且非 demo | `artifacts/preflight/real-entry-proof.json` |
| `publish_ok` | apply 是否成功，且 publish/readback 证据齐全 | `artifacts/live_acceptance/publish-audit-*.json`、`readback-*.json` |
| `rollback_ok` | rollback 是否成功 | `artifacts/live_acceptance/rollback-*.json` |
| `game_verify_ok` | 游戏内 `visible/editable/saveable` 是否全部完成 | 游戏内截图或记录 |
| `evidence_complete` | 自动化与人工强制证据是否全部落盘 | `ac6-data-manager.m1_5-evidence-index.md` |

## 2. 判定优先级

| 优先级 | 条件 | verdict |
|---|---|---|
| 1 | `huorong_matrix` 任一步骤 = `blocked` | `fail` |
| 2 | `preflight_ok = false` | `fail` |
| 3 | `real_entry_ok = false` | `fail` |
| 4 | `publish_ok = false` | `fail` |
| 5 | `rollback_ok = false` | `fail` |
| 6 | `evidence_complete = false` | `release-pending` |
| 7 | `publish_ok` / `rollback_ok` 还没有真机结果（`None` / 缺失） | `release-pending` |
| 8 | `game_verify_ok = false` | `release-pending` |
| 9 | 上述条件全部满足，且 `huorong_matrix` 至少一项 = `manual-allow` | `conditional-pass` |
| 10 | 上述条件全部满足，且 `huorong_matrix` 全部 = `pass` | `live-pass` |

## 3. 速查矩阵

| blocked | preflight_ok | real_entry_ok | publish_ok | rollback_ok | game_verify_ok | evidence_complete | manual-allow present | verdict |
|---|---|---|---|---|---|---|---|---|
| 是 | 任意 | 任意 | 任意 | 任意 | 任意 | 任意 | 任意 | `fail` |
| 否 | 否 | 任意 | 任意 | 任意 | 任意 | 任意 | 任意 | `fail` |
| 否 | 是 | 否 | 任意 | 任意 | 任意 | 任意 | 任意 | `fail` |
| 否 | 是 | 是 | 否 | 任意 | 任意 | 任意 | 任意 | `fail` |
| 否 | 是 | 是 | 是 | 否 | 任意 | 任意 | 任意 | `fail` |
| 否 | 是 | 是 | 是/缺失 | 是/缺失 | 否 | 任意 | 任意 | `release-pending` |
| 否 | 是 | 是 | 缺失 | 缺失 | 任意 | 否 | 任意 | `release-pending` |
| 否 | 是 | 是 | 是 | 是 | 是 | 是 | 是 | `conditional-pass` |
| 否 | 是 | 是 | 是 | 是 | 是 | 是 | 否 | `live-pass` |

## 4. 使用说明

1. 先填完 checklist 与 ZPX-GE77 live acceptance record。
2. 再核对 Huorong matrix、publish/readback/rollback 证据、游戏内复核是否齐全。
3. 最后按本表输出 verdict；若证据缺失，不得跳过 `release-pending` 直接宣称 `live-pass`。
