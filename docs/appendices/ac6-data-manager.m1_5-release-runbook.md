# AC6 Data Manager M1.5 Release Runbook

## 1. 目的

指导 Windows x64 one-folder portable release 的 build、preflight、zip、ZPX-GE77 live acceptance 与最终 verdict 归档。本文只覆盖 `M1.5：Windows EXE Packaging & Live Acceptance`，不扩展 M2/M3。

## 2. 硬约束

1. 当前产物必须来自显式 5-worker 执行车道；若实际 worker pane 少于 5，则该轮执行直接判定失败。
2. AC mutation 继续禁止；五个 AC gate 保持 locked。
3. 所有 save write 必须继续满足：
   - `dry-run before apply`
   - `shadow workspace`
   - `post-write readback`
   - `no in-place overwrite`
   - `fresh-destination-only`
   - `fail-closed`
4. packaged exe 默认入口必须是稳定服务链路；`FixtureGuiDataProvider` 仅允许显式 `--demo` 或开发模式。
5. `third_party/WitchyBND/**` 只能使用 release root 内 sidecar，不允许退回系统 PATH。

## 3. 输入物与前置条件

- `third_party/WitchyBND/` baseline 目录
- 本次 build 的 `version-label`
- 新的 portable zip 输出目录
- `ZPX-GE77 + Windows 10 25H2 x64 + Huorong enabled`
- 新的 apply / rollback 目标目录
- 可归档的证据目录：
  - `artifacts/release/`
  - `artifacts/preflight/`
  - `artifacts/live_acceptance/`
  - `artifacts/huorong/`

## 4. Phase A：构建 portable release

```bash
python scripts/build_windows_portable.py \
  --third-party-source <WitchyBND baseline dir> \
  --version-label <label> \
  --skip-pyinstaller
```

说明：

- 若不加 `--skip-pyinstaller`，脚本会调用 `PyInstaller --noconfirm packaging/pyinstaller.spec`。
- 脚本会自动产出：
  - `artifacts/release/content-manifest.json`
  - `artifacts/release/evidence-manifest.json`
  - `artifacts/preflight/third-party-report.json`
  - `artifacts/preflight/preflight-report.json`
  - `artifacts/preflight/preflight-report.txt`
  - `artifacts/preflight/real-entry-proof.json`
- builder 默认把 `evidence-manifest.json` 标为 `release-pending`，因为它不能伪造 ZPX-GE77 live evidence。

## 5. Phase B：仓内 smoke / preflight 基线

推荐先跑仓内 baseline，再去真机验收：

```powershell
powershell -File scripts/smoke_test_portable.ps1 -ReleaseRoot "artifacts/release/AC6 saving manager"
```

```bash
python scripts/run_preflight_check.py \
  --release-root artifacts/release/AC6\ saving\ manager \
  --expected-version-label <label> \
  --json-out artifacts/preflight/preflight-report.json \
  --text-out artifacts/preflight/preflight-report.txt \
  --provider-proof-out artifacts/preflight/real-entry-proof.json
```

通过条件：

- `preflight-report.json.overall_status == "pass"`
- `real-entry-proof.json.provider_kind == "service_bundle"`
- `real-entry-proof.json.is_demo == false`
- `third-party-report.json.success == true`

## 6. Phase C：打包 zip

```bash
python scripts/create_release_zip.py \
  --release-root artifacts/release/AC6\ saving\ manager \
  --output-zip artifacts/release/AC6-saving-manager-portable.zip
```

归档：

- portable zip 文件
- zip SHA256
- 本轮 `version-label`
- 对应 `content-manifest.json`

## 7. Phase D：ZPX-GE77 live acceptance 执行

### 7.1 初始化手工采集模板

```powershell
powershell -File scripts/live_acceptance_capture.ps1 `
  -ReleaseRoot "artifacts/release/AC6 saving manager" `
  -OutputDir "artifacts/live_acceptance"
```

确认 `artifacts/live_acceptance/checklist-manual.md` 已生成。

### 7.2 解压与首启

1. 解压 portable zip 到新的空目录。
2. 执行：

   ```powershell
   .\AC6 saving manager.exe --provider-proof-json artifacts\preflight\real-entry-proof.json
   ```

3. 保存首启截图。
4. 记录 Huorong 对 unzip / first launch 的结论。

### 7.3 preflight 与 sidecar gate

执行 `scripts/run_preflight_check.py`，确认：

- `release-layout=pass`
- `third-party-manifest=pass`
- `runtime-entry=pass`
- `publish-contract=pass`
- `offline-readiness=pass`

若任一项不是 `pass`，立即停止并按 `fail` 处理。

### 7.4 Stable lane 主路径

1. 浏览 share emblem。
2. 生成 ImportPlan。
3. 先 dry-run。
4. apply 到新的 fresh destination。
5. 确认生成：
   - `artifacts/live_acceptance/publish-audit-*.json`
   - `artifacts/live_acceptance/readback-*.json`
   - `artifacts/live_acceptance/incident/incident-*.json`（若失败）

判定规则：

- 任何原地覆盖 source save 的尝试都属于 `fail`
- 缺少 readback 证据则不得进入 live-pass / conditional-pass

### 7.5 Rollback Drill

1. 使用新的 fresh destination 进行 rollback。
2. 确认生成 `artifacts/live_acceptance/rollback-*.json`。
3. 记录 Huorong 对 rollback 的结论。

### 7.6 二次启动与游戏内复核

1. second launch。
2. 在游戏内完成：
   - `visible`
   - `editable`
   - `saveable`
3. 保存截图或操作记录。

## 8. Phase E：Huorong 与证据归档

最少归档以下内容：

- `artifacts/huorong/huorong-matrix.*`
- `artifacts/huorong/screenshots/`
- `artifacts/huorong/notes.md`
- `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`

记录要求：

- 每一步必须是 `pass` / `manual-allow` / `blocked` 之一。
- 若出现 `manual-allow`，要写明弹窗对象、允许范围、是否只出现一次。
- 若出现 `blocked`，直接终止后续结论升级，并把 verdict 记为 `fail`。

## 9. Verdict 计算

按 `src/ac6_data_manager/release/evidence.py::derive_release_verdict` 的规则执行：

1. 任一步骤 Huorong = `blocked` → `fail`
2. `preflight_ok=false` 或 `real_entry_ok=false` → `fail`
3. `publish_ok=false` 或 `rollback_ok=false` → `fail`
4. 证据不完整、publish/rollback 缺失、或游戏内复核未完成 → `release-pending`
5. 证据完整且存在 `manual-allow` → `conditional-pass`
6. 证据完整且 Huorong 全部 `pass` → `live-pass`

详表见：`docs/appendices/ac6-data-manager.m1_5-release-verdict-truth-table.md`

## 10. 停止条件与回滚

出现以下任一情况，立即停止 live acceptance 并保留当前证据：

- Huorong 返回 `blocked`
- provider proof 落入 demo/fixture provider
- preflight 返回 `blocked`
- apply / rollback 目标路径不是 fresh destination
- readback 失败
- second launch 或游戏内复核失败

停止后动作：

1. 保留已有 `publish-audit` / `readback` / `rollback` / `incident` 证据。
2. 更新 `ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`。
3. 依据真值表给出 `fail` 或 `release-pending`，不得宣称 `live-pass`。

## 11. 当前结论

本仓库已达到 `live-acceptance-ready`，但在缺少 `ZPX-GE77 + Huorong enabled` 真机证据之前，最终发布结论仍必须保持 `release-pending`。
