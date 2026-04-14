# AC6 Data Manager M1.5 Live Acceptance Checklist

- 状态：Approved Draft
- 适用阶段：M1.5
- 验收机：`ZPX-GE77`

## 1. 预检查
- [ ] 使用 Windows 10 25H2 x64
- [ ] 火绒处于日常启用状态
- [ ] 断网或确认本流程不依赖联网
- [ ] release 目录完整
- [ ] `third_party/WitchyBND/` 存在
- [ ] `third_party_manifest.json` 存在

## 2. 解压与首启
- [ ] 解压 portable zip
- [ ] 记录 zip 解压是否被火绒拦截
- [ ] 启动 `AC6 saving manager.exe`
- [ ] 记录首启截图
- [ ] 记录启动日志
- [ ] 确认当前 provider 不是 demo

## 3. Preflight
- [ ] 运行 preflight
- [ ] 记录 OS / arch 检查结果
- [ ] 记录 sidecar manifest gate 结果
- [ ] 记录 output path policy 检查结果
- [ ] 记录 offline readiness 结果

## 4. Emblem Stable Lane 主路径
- [ ] 打开测试存档
- [ ] 浏览 emblem library
- [ ] 手动选定 share emblem
- [ ] 生成 ImportPlan
- [ ] 执行 dry-run
- [ ] 执行 apply 到 fresh destination
- [ ] 检查 post-write readback
- [ ] 检查 audit artifact
- [ ] 检查 incident artifact（如有）

## 5. Rollback Drill
- [ ] 基于 restore point 执行 rollback
- [ ] rollback 输出路径为 fresh destination
- [ ] rollback 后 hash/readback 通过
- [ ] 记录 rollback 结果

## 6. Huorong 维度
| 步骤 | 结果（pass/manual-allow/blocked） | 证据 |
|---|---|---|
| zip 解压 |  |  |
| exe 首启 |  |  |
| preflight |  |  |
| sidecar 调用 |  |  |
| dry-run |  |  |
| apply |  |  |
| rollback |  |  |
| 二次启动 |  |  |

## 7. 游戏内复核
- [ ] visible
- [ ] editable
- [ ] saveable
- [ ] 记录人工复核结果

## 8. Release Verdict
- [ ] Pass
- [ ] Conditional Pass
- [ ] Fail
- [ ] 若非 Pass，已写入 Known Limitations / Operator Guide
