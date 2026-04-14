# AC6 Data Manager M1.5 Huorong Risk Notes

## 1. 风险对象
- `AC6 saving manager.exe`
- `third_party/WitchyBND/`
- portable zip
- preflight / publish / rollback 输出目录

## 2. 结果映射
- `pass`：不阻断关键流程
- `manual-allow`：需人工允许，但允许后流程稳定通过
- `blocked`：关键流程无法继续，直接判定 fail

## 3. 当前状态
本会话仅完成仓内实现与自动化验证，未在 `ZPX-GE77 + Huorong enabled` 上执行真实记录，因此当前发布判定保持 `release-pending`。
