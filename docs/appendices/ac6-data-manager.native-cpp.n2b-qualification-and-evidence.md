# AC6 Data Manager Native C++ N2B Qualification & Evidence

- 状态：Implemented / Automated Qualification Passed / Live Acceptance Pending
- 阶段：N2B
- 最后更新：2026-04-15
- 关联 PRD：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`
- 关联验收基线：`./ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`

## 1. 目标

记录 `.ac6emblemdata` 在 N2B 阶段的 qualification 结果与证据索引，确保“导出所选徽章 / 导入徽章文件到使用者栏”已进入当前构建与测试基线。

## 2. 通过清单

- [x] `.ac6emblemdata` 扩展名与格式版本固定
- [x] GUI 存在 `导出所选徽章`
- [x] GUI 存在 `导入徽章文件到使用者栏`
- [x] 文件导入复用目标使用者栏位弹窗
- [x] import 输出 fresh destination
- [x] post-write readback 成功
- [x] reopen save 成功

说明：
- 最后一项当前主要依赖已存在的 native save shadow workspace / repack / reopen 工作流；exchange-file 专属 live probe 仍建议继续补录。

## 3. 证据记录

### 3.1 Export 记录
- 状态：`PASS`
- 输入存档：native test 构造的 share emblem 目录项
- 资产 ID：`share-emblem`
- 输出文件：`share-mark.ac6emblemdata`
- 扩展名检查：由 `ExchangePackageCodecTest.EmblemRoundTripPreservesStableFields` 与 `NativeExchangeWorkflowTest.ExportEmblemWritesAc6EmblemDataPackage` 覆盖
- 版本字段检查：固定 `format_version = 1`
- 校验信息：checksum mismatch / unsupported format version fail-closed 已有自动化覆盖
- 证据路径：
  - `artifacts/acceptance/n2b/export-emblem.txt`
- 结论：export service 已接通。

### 3.2 Import 记录
- 状态：`PASS`
- 输入交换文件：`.ac6emblemdata`
- 目标存档：真实存档 shadow workspace
- 目标使用者栏位：显式用户选择
- 输出 `.sl2`：fresh destination
- readback 结果：由既有 emblem import constraints tests 覆盖
- reopen 结果：沿用 N2A 已验证的 repack/open-save 工作流
- 证据路径：
  - `artifacts/acceptance/n2b/import-emblem.txt`
- 结论：exchange import 已复用现有徽章导入执行链。

### 3.3 Fail-Closed 记录

- 场景：坏扩展名 / 坏 checksum / 坏 payload schema / 非法目标栏位 / 版本不兼容
- 预期 fail-closed 行为：拒绝读取或拒绝生成导入计划，不写任何目标文件
- 实际结果：
  - package codec 已对扩展名、checksum、format version 执行 fail-closed
  - import constraints 已对非法目标栏位执行 fail-closed
  - exchange workflow 已经复用 codec 的 payload schema 校验
- 证据路径：
  - `artifacts/acceptance/n2b/export-emblem.txt`
  - `artifacts/acceptance/n2b/import-emblem.txt`
- 是否阻塞发布：否，当前阻塞仅剩真人 GUI/live acceptance 证据补录。

## 4. 建议证据路径

- `artifacts/acceptance/n2b/export-emblem.txt`
- `artifacts/acceptance/n2b/import-emblem.txt`
- `artifacts/acceptance/n2b/gui/export-selected-emblem.png`
- `artifacts/acceptance/n2b/gui/import-emblem-file-to-user-slot.png`
- `artifacts/acceptance/n2b/gui/target-slot-dialog-from-exchange.png`

## 5. 发布结论

- 当前结论：`AUTOMATED PASS / LIVE ACCEPTANCE PENDING`
- 剩余工作：补真人 GUI 截图与一条 exchange-file 专属 live probe 产物。
