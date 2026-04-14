# AC6 Data Manager M1.5 Live Acceptance Harness

## 1. 目标
为 Worker3 负责的 publish / readback / rollback / incident evidence 提供统一落盘入口，避免 live acceptance 阶段只留下离散文件而缺少可审计索引。

## 2. 范围
- 保持 `fresh-destination-only`、`post-write readback`、`fail-closed` 契约可回溯。
- 把 publish audit、readback report、rollback report、incident artifact、Huorong 结果与人工 checklist 收敛到同一份 evidence manifest。
- 不伪造 `ZPX-GE77` 真机结果；缺少实机证据时 verdict 仍必须保持 `release-pending`。

## 3. 工具
### 3.1 验收模板准备器
- `scripts/prepare_live_acceptance_capture.py`
- 作用：生成三类模板产物：
  - `artifacts/live_acceptance/checklist-manual.md`
  - `artifacts/live_acceptance/capture-context.json`
  - `artifacts/huorong/huorong-matrix-template.json`
- 约束：`release-root` 必须存在；模板内继续显式声明 `fresh-destination-only`、`post-write readback`、`fail-closed` 合同。

### 3.2 Windows 手工步骤模板
- `scripts/live_acceptance_capture.ps1`
- 作用：在 Windows 验收机上快速生成基础 checklist；若需要完整契约与 Huorong 模板，优先使用 Python 准备器。

### 3.3 证据索引写入器
- `scripts/record_live_acceptance_evidence.py`
- 作用：把已存在的 live acceptance 证据路径写回 `artifacts/release/evidence-manifest.json` 或指定输出文件。
- 约束：所有通过参数传入的路径都必须已存在；缺失路径直接失败，防止把不存在的证据写入 manifest。

## 4. 推荐命令
```bash
python scripts/prepare_live_acceptance_capture.py \
  --release-root artifacts/release/AC6\ saving\ manager \
  --output-dir artifacts/live_acceptance \
  --huorong-dir artifacts/huorong \
  --machine ZPX-GE77

python scripts/record_live_acceptance_evidence.py \
  --release-root artifacts/release/AC6\ saving\ manager \
  --release-content-manifest artifacts/release/content-manifest.json \
  --output-path artifacts/release/evidence-manifest.json \
  --third-party-report artifacts/preflight/third-party-report.json \
  --preflight-report artifacts/preflight/preflight-report.json \
  --runtime-proof artifacts/preflight/real-entry-proof.json \
  --publish-audit artifacts/live_acceptance/publish-audit-<stamp>.json \
  --readback-report artifacts/live_acceptance/readback-<stamp>.json \
  --rollback-report artifacts/live_acceptance/rollback-<stamp>.json \
  --incident-artifact artifacts/live_acceptance/incident-<stamp>.json \
  --huorong-matrix artifacts/huorong/<matrix>.json \
  --checklist-capture artifacts/live_acceptance/checklist-manual.md \
  --verdict release-pending \
  --note "ZPX-GE77 live evidence collected; waiting game verify"
```

## 5. 验收
- 模板准备器必须生成：
  - `checklist-manual.md`
  - `capture-context.json`
  - `huorong-matrix-template.json`
- `capture-context.json` 内必须保留 publish/rollback/readback/incident 的证据位点说明与 contract 快照。
- `evidence-manifest.json` 中必须出现：
  - `publish_audit`
  - `readback_report`
  - `rollback_report`
  - `incident_artifact`
  - `huorong_matrix`
  - `checklist_capture`
- 任一已声明路径不存在时，脚本必须返回非零退出码。
- 未完成游戏内复核前，verdict 不得擅自写成 `live-pass`。
