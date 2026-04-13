# AC6 Data Manager ADR

- 状态：Accepted
- 版本：v1.0-baseline
- 最后更新：2026-04-14

## 1. Decision

采用以下总体方案作为 AC6 Data Manager 的批准基线：

- `headless save-domain kernel + late-bound GUI shell`
- `EmblemAsset` / `AcAsset` 分型建模，仅共享薄接口
- Emblem stable lane 优先
- AC 先 read-only，再经 5 个量化 gate 解锁 experimental import
- 若 AC 关键门槛长期未闭合，stable 只发布 Emblem Manager + AC Read-only Explorer

## 2. Drivers

1. 本地证据强度集中在徽章链路，AC mutation 仍有大量未知项。
2. 存档写回属于高破坏性能力，必须以验证与回滚优先。
3. 用户明确要求完整、美观 GUI，但这不能凌驾于内核安全与证据边界之上。
4. 社区与官方信号表明，依赖一致性、删除保护、预览来源与工具链安全性都是关键约束。

## 3. Alternatives Considered

### 3.1 直接做 C++/Qt 单体桌面应用
优点：复用现有 C++ 代码快。  
缺点：容易过早冻结壳层与 AC 语义边界。  
结论：不采用为当前基线表述。

### 3.2 统一 AC/徽章资产模型
优点：表面上结构统一。  
缺点：容易把徽章经验外推成 AC 事实。  
结论：不采用。

### 3.3 Headless kernel + late-bound shell
优点：能把高风险 mutation 与 GUI 分离；适配 gated AC 路线；支持降级发布。  
缺点：前期文档与边界设计要求更高。  
结论：采用。

## 4. Why Chosen

该方案最能同时满足以下目标：
- 不虚假承诺 AC mutation 稳定性
- 先稳定交付 Emblem 价值
- 允许 AC 以证据驱动方式逐步开放
- 给 GUI 保留足够自由度，同时不牺牲内核安全

## 5. Consequences

### 正向影响
- 风险被 gate、lane、验证矩阵与降级发布策略显式约束
- 后续 ralph/team 执行时边界更清晰
- 即使 AC 长期未闭合，也能发布有价值产品

### 负向影响
- 文档体系比单文件更复杂
- AC stable mutation 推进速度会慢于单体式冒进方案
- 需要持续维护 gate 证据、治理与审计要求

## 6. Follow-ups

1. 先执行 M0、M0.5、M1
2. 固化 AC reverse artifacts 与 gate 证据产物目录
3. 建立 golden corpus、incident bundle 与 rollback 演练流程
4. 在 M1 后按治理附录触发 shell freeze review
