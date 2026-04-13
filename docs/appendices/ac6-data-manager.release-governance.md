# AC6 Data Manager Release Governance

- 状态：Approved Planning Baseline
- 版本：v1.0-baseline
- 最后更新：2026-04-14
- 关联主文档：
  - `../ac6-data-manager.prd.md`
  - `../ac6-data-manager.technical-design.md`

## 1. 文档边界

本文档定义发布治理、事故等级、experimental -> stable 升级要求、shell freeze review 责任与已知限制管理规则。

## 2. Stable / Experimental 事故等级定义

### 2.1 P0
满足任一条件即视为 P0：
- 不可恢复损档
- 回滚失败导致原始存档无法恢复
- 游戏内不可读档
- 稳定能力造成批量关键依赖错位

### 2.2 P1
满足任一条件即视为 P1：
- experimental 功能导致可复现的关键依赖大面积丢失
- AC 导入成功但不可编辑，且回滚与预警未按设计工作
- 同类高风险缺陷在连续两个版本周期内重复出现

### 2.3 P2
- 功能可回滚但存在明显体验缺陷
- 兼容性报告、风险提示、预览策略说明错误但未造成损档

## 3. AC experimental -> stable 审批要求

AC mutation 从 experimental 升级为 stable 时，审批包至少包含：

1. 连续两个版本周期无 P0 / P1
2. 最新 golden corpus 回归结果
3. 最新 round-trip 报告
4. incident 统计与处置结论
5. 已知限制清单
6. 5 个 AC gate 的维持性证据
7. Data / Visual Fidelity 最新试评结果

未满足上述条件，不得将 AC mutation 升为 stable。

## 4. Shell freeze review 责任与产物

### 4.1 Freeze 触发条件
仅当以下条件全部成立时，才进入 shell freeze review：
- M1 完成
- `preview provenance proven` 已明确策略分派
- 领域 API 连续 2 轮原型无破坏性变更
- 已完成至少一轮 Data / Visual Fidelity 试评

### 4.2 Freeze Review 责任点
- Architect：评估壳层与内核边界是否稳定
- Designer：评估 GUI 信息架构与视觉路径是否收敛
- Test Engineer / Verifier：评估 UI 路径是否支持后续验收

### 4.3 Freeze Review 产物
- 壳层选择结论
- 备选方案淘汰理由
- 未冻结风险列表
- 后续变更限制清单

## 5. 已知限制清单模板

每次发布应维护一份“已知限制”列表，至少包含：
- 限制标题
- 所属能力（Stable / Experimental）
- 影响范围
- 触发条件
- 临时规避方式
- 计划解决阶段

## 6. 版本兼容声明规则

每次包导入、导出与发版声明应明确：
- 对应游戏补丁区间
- 对应工具链版本
- 是否依赖 experimental 语义
- 是否存在降级预览或降级导入路径
- 是否有已知限制条目
