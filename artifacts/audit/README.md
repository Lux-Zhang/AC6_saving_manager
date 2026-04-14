# Audit Baseline

每条审计事件至少记录：
- 时间
- save hash
- workspace id
- toolchain version
- rule set version
- plan summary
- result status

## 事件类型
- `dry_run`
- `apply`
- `rollback`
- `blocked_action`
- `incident_bundle`

## 结果状态
- `planned`
- `passed`
- `blocked`
- `failed`
- `rolled_back`

## 约束
- 高风险失败必须生成 incident bundle。
- blocked action 不得继续推进 Apply。
- 审计日志必须可回放到 restore point / rollback 入口。
