# Incident Bundle Baseline

incident bundle 用于保存 fail-closed 证据，至少包含：
- `incident.json`
- 触发该事件的请求快照
- 最小可复现上下文
- 风险说明与处理建议

## 不允许内容
- 明文凭据
- 用户隐私数据
- 原始玩家存档本体

## 推荐命名
- `incident-<uuid>/incident.json`
- `incident-<uuid>/rollback_request.json`
