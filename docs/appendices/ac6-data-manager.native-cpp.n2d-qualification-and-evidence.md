# AC6 Data Manager Native C++ N2D Qualification & Evidence

- 状态：Implemented / Automated Qualification Passed / Packaged Probe Passed
- 阶段：N2D
- 最后更新：2026-04-15
- 关联 PRD：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`
- 关联验收基线：`./ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`
- 前置条件：`docs/appendices/ac6-data-manager.native-cpp.n2c-ac-gate-record.md` 三个 gate 均已 PASS

## 1. 目标

记录 `.ac6acdata` 与 AC/徽章双域 GUI 收口后的 qualification 证据。

## 2. 通过清单

- [x] `.ac6acdata` 扩展名与格式版本固定
- [x] GUI 已存在 `将所选AC导入使用者栏`、`导入AC文件到使用者栏`、`导出所选AC`
- [x] AC 文件导入复用目标使用者栏位弹窗
- [x] export -> import plan -> readback 原语在真实多记录模型下闭环
- [x] GUI 中 AC 与徽章动作文案不混淆
- [x] 多 AC record parser 已纠正 `USER_DATA002..006 = 5 个 AC` 的错误前提

说明：
- 当前 AC 预览仍以 provenance state 为主，不把“原生预览位图已解码”作为 N2D 的前置条件。
- 徽章预览的 shape-id 全映射仍未完成，但不阻塞 `.ac6acdata`、share AC -> user AC、以及双域 GUI 收口。

## 3. 证据记录

### 3.1 AC Export
- 状态：`PASS`
- 自动化覆盖：
  - `ExchangePackageCodecTest.AcRoundTripPreservesStableFields`
  - `NativeExchangeWorkflowTest.ExportAcWritesAc6AcDataPackage`
- 结果：
  - 输出扩展名固定为 `.ac6acdata`
  - `payload_schema = ac.record.raw.v1`
  - `editable_clone_patch_version = ac-location-copy-v1`

### 3.2 AC Import
- 状态：`PASS`
- 自动化覆盖：
  - `AcCatalogContractTest.GateAc03ShareCopyAppendsOneRecordAndReadbackMatches`
  - `NativeExchangeWorkflowTest.AcExchangeImportPlanAcceptsQualifiedRecordModel`
- 结果：
  - share AC 与 `.ac6acdata` 均要求显式目标使用者栏位
  - 支持 replace / append next free
  - 对 hole target fail-closed
  - readback 绑定到真实 recordRef

### 3.3 GUI Finalization
- 状态：`PASS`
- 自动化覆盖：
  - `HomeLibraryViewTest.ExposesDualLibraryTabsAndActionStates`
  - `TargetSlotDialogTest.ProvidesFourTargetSlotsAndRequiresOutputPath`
  - `TargetSlotDialogTest.SupportsPackageOriginForExchangeImport`
- 结果：
  - 徽章库 / AC库 双 tab 并存
  - 三个 AC 动作按钮已启用
  - AC 详情区文案已切到 `AES-CBC begin/end record` 模型

### 3.4 Packaged Probe
- 状态：`PASS`
- 命令：
  - `artifacts/release/native-win-x64-current/AC6 saving manager.exe --probe-open-save artifacts/witchy_probe/AC60000.sl2`
- 结果：
  - `workflowAvailable=true`
  - `hasRealSave=true`
  - `catalogCount=266`
- 结论：
  - packaged exe 已载入新的多记录 AC parser，而不是旧的固定 320 AC 伪模型。

## 4. 建议证据路径

- `build/native/tests/Release/ac6dm_native_tests.exe`
- `artifacts/release/native-win-x64-current/AC6 saving manager.exe`
- `artifacts/witchy_probe/AC60000.sl2`

## 5. 发布结论

- 当前结论：`AUTOMATED PASS / PACKAGED PROBE PASS`
- 非阻塞后续项：
  - 继续补真人 GUI 截图
  - 继续扩充徽章 preview 的 shape-id -> geometry 映射
