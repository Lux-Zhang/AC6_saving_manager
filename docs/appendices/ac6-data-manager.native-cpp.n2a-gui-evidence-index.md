# AC6 Data Manager Native C++ N2A GUI Evidence Index

- 状态：Implemented / Evidence indexed / Handoff to N2B-N2D lanes
- 最后更新：2026-04-15（N2A 完成，转入 N2B-N2D 文档接力）
- 关联阶段：N2A
- 关联 PRD：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`
- 关联验收基线：`./ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`

## 1. GUI 证据目标

本索引用于收纳 N2A GUI / validation lane 需要提交给主 agent 的证据定位，当前只冻结证据目录结构与检查点，不替代最终人工验收记录。

## 2. 本 lane 负责的证据点

1. `徽章库 / AC库` 双视图入口存在。
2. `将所选徽章导入使用者栏` 入口存在。
3. 目标使用者栏位选择弹窗存在，且提供 `使用者1/2/3/4`。
4. AC 相关动作以 disabled 形式可见，且 GUI 中存在明确 reason。
5. GUI 自动化测试覆盖以上入口与状态。

## 3. 建议证据文件

- 截图：
  - `artifacts/acceptance/n2a/gui/emblem-library.png`
  - `artifacts/acceptance/n2a/gui/ac-library.png`
  - `artifacts/acceptance/n2a/gui/target-slot-dialog.png`
- 测试输出：
  - `artifacts/acceptance/n2a/tests/gui-home-library-view.txt`
  - `artifacts/acceptance/n2a/tests/gui-target-slot-dialog.txt`

## 4. 当前已冻结的测试名

- `HomeLibraryViewTest.ExposesDualLibraryTabsAndActionStates`
- `TargetSlotDialogTest.ProvidesFourTargetSlotsAndRequiresOutputPath`

## 5. 主 agent 后续补充项

1. 当 catalog / preview / emblem mutation lane 完成接线后，补拍真实 AC 列表与 preview provenance 截图。
2. 当指定目标栏位写入接口完成后，补齐弹窗选择与 readback 成功的全链路证据。
3. 在 Verifier 轮次把本索引中的建议路径替换为实际产物路径。

## 6. 当前轮已落地的实际证据

- 原生构建与回归输出：
  - `对话记录与任务日志/jobs/JOB-omx-1776116945771-85s7n9/命令输出/20260415-084659_n2a_build_and_test_retry3.txt`
- 真实样本 open-save probe：
  - `artifacts/probe_ascii/probe_stdout_release.txt`
- 真实样本 import probe：
  - `artifacts/probe_ascii/probe_import_stdout_release.txt`
- 探针输出存档：
  - `artifacts/probe_ascii/AC60000-n2a-import.sl2`

## 7. 当前轮结论摘录

1. `HomeLibraryViewTest.ExposesDualLibraryTabsAndActionStates` 通过。
2. `TargetSlotDialogTest.ProvidesFourTargetSlotsAndRequiresOutputPath` 通过。
3. N2A 原生测试子集 `17/17` 通过。
4. 真实样本 probe 表明：
   - 可打开真实存档；
   - catalog 现为 `116` 项；
   - 可执行分享徽章导入并输出新存档；
   - reopen probe 成功。


## 8. 向 N2B / N2C / N2D 的交接要求

### N2B 需复用的 N2A 证据前提
- `TargetSlotDialog` 已是 GUI 主路径，不允许在文件导入场景回退成自动选位。
- N2B 的 `导入徽章文件到使用者栏` 必须沿用同一弹窗与同一输出存档约束。

### N2C 需复用的 N2A 证据前提
- `AC库` 已存在真实目录视图；N2C 必须在此基础上补 gate，而不是另起临时 probe-only 入口。
- preview provenance 状态必须继续在 GUI 可见。

### N2D 需复用的 N2A 证据前提
- AC 用户动作启用前，必须引用 N2C gate 文档中的 PASS 结论。
- 最终 GUI 收口时，不得丢失 N2A 已验证的双视图与 disabled reason 语义。
