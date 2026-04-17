# AC6 Data Manager Native C++ N2C AC Gate Record

- 状态：PASS / Qualified On Multi-Record Model
- 阶段：N2C
- 最后更新：2026-04-15
- 关联 PRD：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`
- 关联验收基线：`./ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md`

## 1. 使用说明

本文件记录 AC 从只读目录进入 N2D 前的 gate 结论。当前 native C++ 主线已经不再把 `USER_DATA002..006` 视为 5 个 top-level payload，而是按 `AES-CBC + 20字节容器头 + begin/end record + 16字节footer` 的真实多记录模型解析并写回。

状态枚举：
- `TODO`
- `RUNNING`
- `PASS`
- `FAIL`
- `BLOCKED`

## 2. Gate 总览

| Gate | 名称 | 当前状态 | 是否阻塞 N2D |
|---|---|---|---|
| Gate-AC-01 | record-map stable | PASS | 否 |
| Gate-AC-02 | preview provenance stable | PASS | 否 |
| Gate-AC-03 | copy/write + readback stable | PASS | 否 |

## 3. Gate-AC-01：record-map stable

- 当前状态：`PASS`
- 样本集：`artifacts/witchy_probe/AC60000-sl2/USER_DATA002..006`
- 自动化命令：`ctest --output-on-failure -C Release`
- 人工检查点：AC 库列表应按 使用者1/2/3/4 + 分享 展示，且每个 bucket 内含多条记录。
- 通过条件：
  - `USER_DATA002..006 -> user1..user4/share` 映射固定
  - 单个 `USER_DATA00X` 内部按多条 AC record 建模
  - recordRef 为 record-level，而不是顶层整文件
- 实测结果：
  - `AcCatalogContractTest.EncryptedBeginEndRecordMapExportsBucketRecords` 覆盖 AES-CBC 解密、容器头 record count、begin/end record 边界与 recordRef 输出。
  - 当前 packaged probe：`artifacts/release/native-win-x64-current/AC6 saving manager.exe --probe-open-save artifacts/witchy_probe/AC60000.sl2` 返回 `catalogCount=266`，说明 AC catalog 已从旧的固定 320 项伪模型收敛到真实多记录结果。
- 证据路径：
  - `build/native/tests/Release/ac6dm_native_tests.exe`
  - `artifacts/release/native-win-x64-current/AC6 saving manager.exe`

## 4. Gate-AC-02：preview provenance stable

- 当前状态：`PASS`
- 样本集：record-level AC catalog items + `native/tests/ac_preview_probe_test.cpp`
- 自动化命令：`ctest --output-on-failure -C Release`
- 人工检查点：AC 详情区显示稳定的 provenance 与 preview state，不再把顶层文件假设映射为原生预览。
- 通过条件：
  - preview provenance 绑定到真实 recordRef
  - 对未解码原生预览的 record，产品层稳定显示 `unknown / ac.record-preview-forensics-only`
- 实测结果：
  - AC catalog 详情字段已输出 `RecordRef / RecordByteOffset / RecordByteLength / ContainerHeaderCount / FirstRecordDesignOffset`
  - GUI 文案改为 begin/end record 模型，不再宣称 fixed-slot
- 备注：
  - 该 gate 的“PASS”指 preview 语义稳定，不等于已经实现 AC 原生预览位图解码。

## 5. Gate-AC-03：copy/write + readback stable

- 当前状态：`PASS`
- 样本集：synthetic encrypted multi-record containers
- 自动化命令：`ctest --output-on-failure -C Release`
- 人工检查点：主动作与文件导入都要求显式目标使用者栏位；允许 replace 或 append，不允许跨越空洞。
- 通过条件：
  - share AC -> 指定 user slot 写入成功
  - `.ac6acdata` 文件导入复用同一 write primitive
  - readback 匹配
  - 输出 fresh destination
- 实测结果：
  - `AcCatalogContractTest.GateAc03ShareCopyAppendsOneRecordAndReadbackMatches`
  - `NativeExchangeWorkflowTest.ExportAcWritesAc6AcDataPackage`
  - `NativeExchangeWorkflowTest.AcExchangeImportPlanAcceptsQualifiedRecordModel`
  - share AC / `.ac6acdata` 都复用 `applyPayloadToUserSlot()` 的 raw record clone + header count rewrite。

## 6. N2D 准入判定

- [x] Gate-AC-01 = PASS
- [x] Gate-AC-02 = PASS
- [x] Gate-AC-03 = PASS
- [x] 现有自动化证据路径可被主 agent 或 verifier 复查

当前 N2D 状态：`PASS`
