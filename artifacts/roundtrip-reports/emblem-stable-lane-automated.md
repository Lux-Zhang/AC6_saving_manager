# Emblem Stable Lane Automated Round-Trip Report

- 日期：2026-04-14
- 范围：M0.5 / M1 仓库自动化证据
- 结论：仓库级 round-trip / rollback / audit / GUI 验证通过；真实 `.sl2` + WitchyBND + 游戏内验证仍待补。

## 1. 运行命令

```text
QT_QPA_PLATFORM=offscreen PYTHONPATH=src pytest tests -q
```

结果：`27 passed`

## 2. 关键验证覆盖

### 2.1 Container / Workspace
- `tests/test_container_workspace.py`
- 覆盖：shadow workspace、restore point、readback、incident、in-place overwrite guard、fake WitchyBND round-trip。

### 2.2 Emblem binary / import / package / preview
- `tests/test_emblem_binary.py`
- `tests/test_emblem_import.py`
- `tests/test_emblem_package.py`
- `tests/test_emblem_preview.py`
- 覆盖：`USER_DATA007` / `EMBC` parse-serialize、share->user selective import、`ac6emblempkg` v1 import/export/apply、preview fail-soft placeholder。

### 2.3 Validation / rollback / audit
- `tests/test_validation_audit.py`
- `tests/test_validation_corpus.py`
- `tests/test_validation_rollback.py`
- `tests/test_validation_baseline.py`
- `tests/test_validation_readiness.py`
- 覆盖：audit JSONL、incident index、golden corpus manifest、rollback、新路径恢复、M2 readiness。

### 2.4 GUI
- `tests/test_gui_models.py`
- `tests/test_gui_windows.py`
- 覆盖：PyQt5 model、主窗口、ImportPlan dialog、DeleteImpactPlan dialog、audit view fixture 初始化。

## 3. 当前仍未覆盖的外部验证

1. 真实 WitchyBND 二进制版本与真实 `.sl2` 的 apply / rollback 演练。
2. 游戏内人工确认 emblem 导入后的显示与编辑行为。
3. AC read-only explorer 的 reverse corpus 与 gate 证据。
