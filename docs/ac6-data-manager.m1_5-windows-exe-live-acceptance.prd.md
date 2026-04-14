# AC6 Data Manager M1.5 PRD

- 文档状态：Approved for Execution
- 版本：v0.9-m1.5
- 最后更新：2026-04-14
- 所属阶段：`M1.5：Windows EXE Packaging & Live Acceptance`
- 关联技术方案：`ac6-data-manager.m1_5-windows-exe-live-acceptance.technical-design.md`
- 关联附录：
  - `appendices/ac6-data-manager.m1_5-live-acceptance-checklist.md`
  - `appendices/ac6-data-manager.m1_5-release-runbook.md`
  - `appendices/ac6-data-manager.m1_5-huorong-risk-notes.md`
  - `appendices/ac6-data-manager.m1_5-third-party-manifest-spec.md`
  - `appendices/ac6-data-manager.m1_5-evidence-index.md`

## 1. 文档目标
本文件用于把已批准的 M1.5 规划收敛为正式产品需求基线，明确以下事项：
- 为什么当前阶段必须先做 Windows EXE 与 live acceptance，而不是继续扩展 M2/M3。
- 当前阶段交付什么、不交付什么。
- 人工验收、失败路径、火绒风险和 release verdict 的统一口径。
- 后续 `omx team` 与 `omx ralph` 执行时的功能、范围与验收边界。

## 2. 背景
当前仓库已经完成 M0、M0.5、M1 的主要仓内实现与验证：
- container workspace / shadow workspace / restore point / readback / rollback 已有实现基础。
- emblem stable lane 已具备目录化与自动化测试基础。
- GUI 已具备可扩展壳层，但打包默认入口仍是 demo provider。
- 目前状态仍停留在：
  - M0：partial
  - M0.5：repo-pass / live-pending
  - M1：repo-pass / release-pending

用户新增的刚性要求是：
- 本轮验收加入人工验收。
- 程序必须编译为 Windows exe。
- 需要便携目录 + exe + 依赖文件 + zip 的交付形态。
- 验收环境固定为 Windows 10 x64，并存在火绒。

因此，下一步不能直接进入 M2；必须先完成 M1.5，解决“可交付、可离线运行、可人工验收、可回滚、可审计”的发布硬化问题。

## 3. 用户与验收环境
### 3.1 目标验收人
- 当前主要验收人：项目发起人本人
- 验收类型：人工验收 + 程序内证据核查 + 游戏内复核

### 3.2 固定验收环境
- 机器标识：`ZPX-GE77`
- 操作系统：Windows 10 25H2
- OS build：`26200.8037`
- 架构：x64 only
- 平台：Steam
- 游戏版本：`1.9.0.0`
- Mod 状态：无 mod
- 安全软件：火绒
- 网络要求：离线可用

### 3.3 交付形态
首版固定为：
- one-folder portable release
- `AC6 saving manager.exe`
- 打包 sidecar 与运行依赖随包提供
- 可另行生成 zip 供整体分发

## 4. 本阶段业务目标
M1.5 的唯一目标是：
把当前已实现的 Emblem stable lane 能力，变成一个可以在 `ZPX-GE77` 上离线启动并按人工清单完成完整验收的 Windows x64 便携版交付物。

对应人工验收主路径：
1. 打开程序
2. 打开测试存档
3. 浏览 emblem library
4. 手动选定 share emblem 导入 user
5. 执行 dry-run / apply
6. 检查 readback / audit
7. 执行 rollback
8. 在游戏中复核

## 5. 范围
### 5.1 In Scope
- Windows x64 打包链路
- PyInstaller one-folder portable release
- WitchyBND pinned sidecar + manifest gate
- preflight 自检
- packaged exe 默认真实 stable-emblem 服务入口
- live acceptance harness
- controlled publish
- rollback drill
- audit / incident artifact / evidence index
- Huorong 风险矩阵
- release runbook 与 known limitations 口径

### 5.2 Out of Scope
- AC import
- AC mutation
- AC experimental import
- AC gate 解锁
- 安装器
- 自动更新
- 单文件 exe
- 跨平台支持
- M2 / M3 功能扩展

## 6. 阶段硬约束
### 6.1 保存写入不变量
所有 save write 必须继续遵守：
- `dry-run before apply`
- `shadow workspace`
- `post-write readback`
- `no in-place overwrite`
- `fail-closed`

### 6.2 Builder 冻结
- 主路径唯一：`PyInstaller one-folder`
- `Nuitka` 只允许作为 contingency，且只有在以下条件同时成立时才可切换：
  1. `ZPX-GE77 + 火绒开启` 条件下出现可复现 blocker；
  2. blocker 经过既定排障预算仍无法解除；
  3. 已有书面归档说明为何主路径不可继续。

### 6.3 Controlled Publish / Rollback 契约
- source save 永不改写。
- apply 只允许：`verified shadow output -> fresh destination only`
- rollback 只允许：`verified rollback artifact -> fresh destination only`
- 目标路径若已存在：必须 fail-closed
- 所有目标路径、时间、结果、readback 结果进入审计证据

### 6.4 Third-party Sidecar 契约
`WitchyBND` 必须：
- 以 `third_party/WitchyBND/**` 目录级 manifest 管理
- 与 `ACVIEmblemCreator bundled version` 对齐
- 不允许系统 PATH fallback
- 对 missing / extra / hash mismatch / version mismatch 一律 fail-closed

### 6.5 默认入口契约
- packaged exe 默认入口必须是真实 stable-emblem 服务链路
- `FixtureGuiDataProvider` 只能显式用于 `--demo` 或开发模式
- smoke/live acceptance 不能退化为“GUI 能打开”

## 7. 用户故事
### US-M1.5-001：离线启动与自检
作为人工验收用户，我希望在未安装 Python、断网环境下直接启动 `AC6 saving manager.exe` 并完成 preflight，以便确认可交付物具备独立可运行性。

### US-M1.5-002：真实 emblem stable lane 验收
作为人工验收用户，我希望在 GUI 中走通 share emblem -> user 的真实导入链路，以便确认当前 stable lane 可以被真实使用，而不是 demo 演示。

### US-M1.5-003：失败路径阻断
作为谨慎型用户，我希望 sidecar 缺失、manifest 漂移、readback mismatch、目标路径已存在等情况被明确阻断，并看到 incident / audit 证据，以便确信程序不会默默破坏存档。

### US-M1.5-004：带火绒环境的可交付判断
作为真实验收用户，我希望知道火绒是否会拦截 exe、zip、sidecar、输出目录或临时文件，以及这类情况是否会阻断发布，以便判断是否可真正交付。

## 8. 功能需求
### 8.1 Packaging Baseline
系统必须支持：
- 生成 Windows x64 one-folder 产物
- 主 exe 命名为 `AC6 saving manager.exe`
- 路径全部相对化，不依赖全局 PATH
- 断网环境可启动

### 8.2 Third-party Governance
系统必须支持：
- WitchyBND 目录级 manifest 校验
- version + sha256 + required files 校验
- preflight 输出第三方依赖状态
- 缺失与漂移时阻断相关操作

### 8.3 Real Stable-Emblem Entry
系统必须支持：
- packaged 默认启动真实 service bundle
- 明确区分 demo/dev 模式与 release 模式
- 对当前 provider 身份留证

### 8.4 Controlled Publish / Rollback
系统必须支持：
- fresh destination only
- publish 前的 readback gate
- rollback 演练
- publish / rollback 审计记录

### 8.5 Huorong-Dimensioned Acceptance
系统必须支持：
- zip 解压、首启、preflight、sidecar、dry-run、apply、rollback、二次启动的状态记录
- 对“通过 / 需人工允许 / 阻断失败”给出统一 verdict 映射

## 9. 里程碑
### M1.5-A Packaging Baseline Freeze
产物：
- 构建脚本与 spec
- one-folder dist/release 目录
- `AC6 saving manager.exe`

### M1.5-B Bundled Dependency Gate
产物：
- `third_party_manifest`
- preflight 校验
- sidecar gate 报告

### M1.5-C Controlled Publish / Rollback Enforcement
产物：
- publish / rollback 契约实现
- fresh destination 阻断证据
- readback / rollback 审计证据

### M1.5-D Real Entry Smoke + Live Acceptance
产物：
- 默认入口真实链路 smoke 记录
- live acceptance 主路径证据

### M1.5-E Huorong Matrix + Release Verdict
产物：
- 火绒风险矩阵
- release verdict
- known limitations / operator guide

## 10. 验收真值表
| 步骤 | 动作 | 期望结果 | 证据文件 | 影响发布结论 |
|---|---|---|---|---|
| 1 | 解压 portable zip | 文件完整，无缺失 | zip 清单 / hash 报告 | 是 |
| 2 | 首次启动 exe | 程序成功进入主界面 | 启动日志 / 截图 | 是 |
| 3 | preflight | 通过或明确阻断 | preflight json / 文本报告 | 是 |
| 4 | sidecar 校验 | WitchyBND gate 通过 | third-party manifest report | 是 |
| 5 | dry-run | 导入计划生成成功 | audit / import plan artifact | 是 |
| 6 | apply 到 fresh destination | 成功或被正确阻断 | publish audit / output manifest | 是 |
| 7 | post-write readback | 读回一致 | readback artifact | 是 |
| 8 | rollback | 成功恢复到 fresh destination | rollback report | 是 |
| 9 | 二次启动复验 | 程序再次稳定启动 | 截图 / 启动日志 | 否 |
| 10 | 游戏内复核 | visible / editable / saveable | 人工复核记录 | 是 |

## 11. Release Verdict
### Pass
同时满足：
- 离线启动通过
- sidecar gate 通过
- dry-run / apply / readback / rollback 全通过
- 火绒未阻断关键流程，或仅出现一次性人工允许且允许后稳定通过
- 证据完整

### Conditional Pass
同时满足：
- 需要人工允许或白名单，但不要求关闭火绒整体保护
- 允许后关键流程稳定通过
- known limitations / operator guide 已明确记录

### Fail
出现以下任一情况：
- 火绒隔离或删除关键文件且无法合理规避
- third-party manifest gate 无法稳定通过
- 默认入口仍落到 demo provider
- apply / rollback 不能保证 fresh destination only
- readback 或 rollback 演练失败

## 12. 执行命令基线
### 推荐 `$team`
```text
$team "按 docs/ac6-data-manager.m1_5-windows-exe-live-acceptance.prd.md 与 docs/ac6-data-manager.m1_5-windows-exe-live-acceptance.technical-design.md 执行 M1.5。Worker1 负责 PyInstaller one-folder Windows x64 portable 打包链路、release 目录契约与 smoke baseline；Worker2 负责 third_party/WitchyBND 目录级 manifest gate、ACVIEmblemCreator baseline 对齐、preflight、自检与 no-PATH-fallback；Worker3 负责 controlled publish / rollback fresh-destination-only 契约、post-write readback、audit / incident artifact 与 live acceptance harness；Worker4 负责 live acceptance checklist、release runbook、Huorong 风险矩阵、evidence index 与 release verdict 真值表；Verifier 负责在 ZPX-GE77 / Windows 10 25H2 / 火绒开启条件下复核 unzip、首启、preflight、sidecar、dry-run、apply、rollback、二次启动全链路证据。继续禁止 AC mutation；五个 AC gate 保持 locked；所有 save write 必须继续遵守 dry-run before apply、shadow workspace、post-write readback、no in-place overwrite、fail-closed；packaged exe 默认入口必须是真实 stable-emblem 服务链路，FixtureGuiDataProvider 仅允许显式 --demo 或开发模式。"
```

### 推荐 `$ralph`
```text
$ralph "基于 M1.5 第一轮产出做串行收口，只整合 Windows x64 one-folder portable release、真实 stable-emblem 默认入口、third_party/WitchyBND 目录级 manifest gate、controlled publish / rollback fresh-destination-only 契约、offline live acceptance 与 Huorong 风险验证。任务：1）统一 release 目录契约与 evidence manifest；2）修正 PyInstaller 打包与运行时缺陷；3）确认默认入口不再落到 demo provider；4）补齐 preflight、publish audit、rollback readback 与 incident artifact；5）完成 ZPX-GE77 真机验收记录、Huorong 结果到 release verdict 的映射、KNOWN_LIMITATIONS 与 runbook；6）确认 M1 是否从 release-pending 提升到 live-pass。继续禁止 AC mutation；五个 AC gate 保持 locked；所有 save write 继续遵守 dry-run before apply、shadow workspace、post-write readback、no in-place overwrite、fail-closed。"
```
