# AC6 Data Manager M1.5 PRD

- 文档状态：Approved for Execution
- 版本：v1.0-m1.5
- 最后更新：2026-04-14
- 所属阶段：`M1.5：Windows EXE Packaging & Live Acceptance`
- 关联技术方案：`ac6-data-manager.m1_5-windows-exe-live-acceptance.technical-design.md`

## 1. 目标
在不触碰 AC mutation、五个 AC gate 与 save write 不变量的前提下，把现有 Emblem stable lane 固化为 Windows x64 one-folder portable release，并补齐 live acceptance 所需的 release/evidence 契约。

## 2. 当前收口结论
本轮仓内实现已经补齐：
- 默认入口改为真实 `ServiceBundleGuiDataProvider`
- `FixtureGuiDataProvider` 仅允许 `--demo` 或开发模式
- `third_party/WitchyBND/**` 目录级 manifest gate
- preflight 汇总、release content manifest、evidence manifest
- publish/rollback 证据输出与 `fresh-destination-only` 发布约束
- Windows one-folder portable release 的 PyInstaller/spec 与打包脚本

但当前会话运行环境不是 `ZPX-GE77 + Windows 10 25H2 + Huorong enabled`，因此不能伪造 live acceptance 结果。当前判定是：
- `M1 = release-ready / live-pending`
- 不能在本会话中提升到 `live-pass`

## 3. In Scope
- Windows x64 one-folder portable release
- 真实 stable-emblem 默认入口
- GUI 必须直接覆盖本轮 live acceptance 要测试的路径：
  - 打开真实 `.sl2` 存档
  - 选择 fresh-destination-only 的 Apply 输出路径
  - 浏览 real-save emblem library
  - 选定 share emblem 并查看 ImportPlan
  - 通过 GUI 执行 dry-run / apply / rollback
  - 通过 GUI 查看审计结果与证据路径
- `third_party/WitchyBND/**` manifest gate 与 no-PATH-fallback
- controlled publish / rollback / readback / incident evidence
- offline preflight / smoke / evidence manifest
- Huorong 风险映射、KNOWN_LIMITATIONS、runbook、ZPX-GE77 验收记录

## 4. Out of Scope
- AC mutation
- 五个 AC gate 解锁
- 安装器、自动更新、单文件 exe、跨平台支持

## 5. 不变量
所有 save write 继续遵守：
- `dry-run before apply`
- `shadow workspace`
- `post-write readback`
- `no in-place overwrite`
- `fresh-destination-only`
- `fail-closed`

## 6. 验收真值表
| 项目 | 要求 | 当前仓内状态 | live-pass 影响 |
|---|---|---|---|
| 默认入口 | 非 demo，provider proof = `service_bundle` | 已实现 + 自动化测试 | 是 |
| sidecar gate | 目录级 manifest + fail-closed | 已实现 + 自动化测试 | 是 |
| publish contract | fresh destination only | 已实现 + 自动化测试 | 是 |
| rollback evidence | 新输出路径 + 审计证据 | 已实现 + 自动化测试 | 是 |
| PyInstaller portable | spec + build script + release layout | 已实现 | 是 |
| ZPX-GE77 live acceptance | 真机记录、Huorong 结果、游戏内复核 | 待执行 | 是 |

## 7. 当前发布判定
只有在以下条件全部补齐后，才能把 M1 从 `release-pending` 提升为 `live-pass`：
1. `ZPX-GE77` 上完成 unzip、首启、preflight、sidecar、dry-run、apply、rollback、二次启动；
2. Huorong 每一步都有 `pass/manual-allow/blocked` 记录；
3. 游戏内 `visible/editable/saveable` 复核完成；
4. 证据文件落到 `artifacts/live_acceptance/`、`artifacts/huorong/` 与 `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`。
