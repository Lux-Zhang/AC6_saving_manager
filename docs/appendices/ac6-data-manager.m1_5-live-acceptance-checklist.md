# AC6 Data Manager M1.5 Live Acceptance Checklist

- 状态：Execution Ready
- 验收机：`ZPX-GE77`

## 1. 预检查
- [ ] Windows 10 25H2 x64
- [ ] Huorong enabled
- [ ] offline path confirmed
- [ ] release root = `artifacts/release/AC6 saving manager/`
- [ ] `third_party/WitchyBND/` 存在
- [ ] `third_party/third_party_manifest.json` 存在

## 2. 首启与 proof
- [ ] `AC6 saving manager.exe --provider-proof-json ...` 输出 `provider_kind=service_bundle`
- [ ] 首启截图保存
- [ ] `preflight-report.json` / `.txt` 保存

## 3. Stable lane 主路径
- [ ] 浏览 share emblem
- [ ] 生成 ImportPlan
- [ ] dry-run
- [ ] apply 到 fresh destination
- [ ] readback evidence 保存
- [ ] publish audit 保存

## 4. Rollback Drill
- [ ] rollback 到 fresh destination
- [ ] rollback evidence 保存

## 5. Huorong Matrix
| 步骤 | 结果（pass/manual-allow/blocked） | 证据 |
|---|---|---|
| unzip |  |  |
| first launch |  |  |
| preflight |  |  |
| sidecar |  |  |
| dry-run |  |  |
| apply |  |  |
| rollback |  |  |
| second launch |  |  |

## 6. 游戏内复核
- [ ] visible
- [ ] editable
- [ ] saveable
