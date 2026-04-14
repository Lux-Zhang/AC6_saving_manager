# AC6 Data Manager M1.5 Release Runbook

## 1. 目的
本 runbook 用于指导 M1.5 的 build、preflight、portable zip、验收记录与 release verdict 产出。

## 2. 输入
- 当前项目根目录：`AC6_saving_manager/`
- 验收基线：`docs/ac6-data-manager.m1_5-windows-exe-live-acceptance.prd.md`
- 技术方案：`docs/ac6-data-manager.m1_5-windows-exe-live-acceptance.technical-design.md`
- 测试存档：上层目录 `../测试存档/`
- WitchyBND baseline：上层目录 `../ACVIEmblemCreator/`

## 3. 预期脚本
- `scripts/build_windows_portable.ps1`
- `scripts/build_windows_portable.py`
- `scripts/verify_third_party_manifest.py`
- `scripts/run_preflight_check.py`
- `scripts/create_release_zip.py`
- `scripts/smoke_test_portable.ps1`
- `scripts/live_acceptance_capture.ps1`

## 4. Build 顺序
1. 清理旧 release 目录
2. 构建 one-folder release
3. 收集 runtime / resources / third_party
4. 写出 `third_party_manifest.json`
5. 运行 preflight
6. 运行 smoke baseline
7. 生成 portable zip
8. 生成 evidence manifest

## 5. 必收集证据
- build log
- release content list
- third-party manifest verification report
- preflight report
- smoke report
- live acceptance checklist
- Huorong matrix
- publish audit
- rollback report
- known limitations

## 6. 失败处理
### 6.1 Build 失败
- 停止发布
- 写出 build failure log
- 不进入 zip 生成

### 6.2 Sidecar Gate 失败
- 停止 preflight 后续动作
- 标记为 fail-closed
- 生成 incident artifact

### 6.3 Real Entry 失败
- 若默认入口仍指向 demo provider，直接阻断发布
- 不进入人工验收

### 6.4 Huorong Blocked
- 若关键文件被删除或隔离且无法可重复规避，判定 release fail

## 7. 输出
- `artifacts/release/`
- `artifacts/preflight/`
- `artifacts/live_acceptance/`
- `artifacts/huorong/`
- `artifacts/incident/`
- `docs/appendices/ac6-data-manager.m1_5-evidence-index.md`
