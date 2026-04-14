# AC6 Data Manager M1.5 Huorong Risk Notes

## 1. 目的
记录火绒在 M1.5 portable release 验收中的观察、阻断条件、缓解方案与 release verdict 映射。

## 2. 风险对象
- `AC6 saving manager.exe`
- `third_party/WitchyBND/`
- portable zip
- preflight / smoke / live acceptance 生成的输出目录
- 临时文件、candidate output、rollback output

## 3. 必测矩阵
| 步骤 | 观察点 | 结果字段 |
|---|---|---|
| zip 解压 | 是否拦截/删除/隔离 | pass/manual-allow/blocked |
| 首启 exe | 是否弹框、拦截、隔离 | pass/manual-allow/blocked |
| preflight | 是否阻断报告生成 | pass/manual-allow/blocked |
| sidecar 调用 | 是否拦截 WitchyBND | pass/manual-allow/blocked |
| dry-run | 是否阻断中间文件 | pass/manual-allow/blocked |
| apply | 是否阻断 candidate output | pass/manual-allow/blocked |
| rollback | 是否阻断 restore / copy | pass/manual-allow/blocked |
| 二次启动 | 是否出现二次隔离或删除 | pass/manual-allow/blocked |

## 4. 结论映射
### Pass
- 无阻断，或仅需一次人工允许且允许后稳定通过。

### Conditional Pass
- 需要加入白名单或进行人工允许，但流程稳定可复现，且操作说明可文档化。

### Fail
- 关键文件被持续隔离或删除，且无法以合理、可重复、可文档化方式规避。

## 5. 文档要求
若出现 `manual-allow` 或 `blocked`，必须更新：
- `KNOWN_LIMITATIONS.txt`
- operator guide / runbook
- evidence index
