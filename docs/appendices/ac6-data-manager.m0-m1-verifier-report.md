# AC6 Data Manager Phase 1 Verifier Report（M0 / M0.5 / M1）

- 状态：已根据当前仓库实现刷新
- 日期：2026-04-14
- 适用范围：仅复核 `M0`、`M0.5`、`M1`
- 当前约束：`AC mutation` 继续禁止；`AC` 仅允许 read-only baseline 与 reverse artifacts 整理
- 复核方式：仓库代码、自动化测试、appendices 与 artifacts 一致性复核；本次未运行真实游戏、未对真实玩家存档执行外部工具写回
- 机器可读索引：`../../artifacts/verification/phase1/m0-m1-verifier-lane.json`

## 1. 结论摘要

1. `M0`：**PARTIAL**。基础设施已具备，但 reverse corpus / docs/reverse 仍未建立。
2. `M0.5`：**REPO-PASS / LIVE-PENDING**。shadow workspace、restore point、post-write readback、incident、rollback、audit 已在仓库中实现，并有自动化测试；但真实 WitchyBND + 真实 `.sl2` + 外部人工验证仍待补。
3. `M1`：**REPO-PASS / RELEASE-PENDING**。emblem 二进制模型、share->user selectable import、`ac6emblempkg` v1、preview、PyQt5 GUI、audit/rollback 已落地并通过自动化回归；但游戏内“可见、可编辑、可保存、预览正确”仍未做实机验证。
4. `AC gate`：5 个 gate 继续保持 `LOCKED`，因此本轮不解锁任何 AC mutation / experimental import。

## 2. 里程碑复核表

| Milestone | 当前结论 | 正向证据 | 阻塞项 |
|---|---|---|---|
| `M0` | `PARTIAL` | `src/ac6_data_manager/container_workspace/*`、`src/ac6_data_manager/validation/corpus.py` 已建立 workspace / corpus helper 基础。 | `docs/reverse/`、真实样本索引、AC 扫描 corpus 未建立。 |
| `M0.5` | `REPO-PASS / LIVE-PENDING` | `tests/test_container_workspace.py`、`tests/test_validation_audit.py`、`tests/test_validation_rollback.py` 覆盖 shadow workspace、restore point、readback、incident、rollback、audit。 | 未对真实 WitchyBND 与真实 `.sl2` 做 end-to-end 演练。 |
| `M1` | `REPO-PASS / RELEASE-PENDING` | `tests/test_emblem_binary.py`、`tests/test_emblem_import.py`、`tests/test_emblem_package.py`、`tests/test_emblem_preview.py`、`tests/test_gui_models.py`、`tests/test_gui_windows.py`；`artifacts/roundtrip-reports/emblem-stable-lane-automated.md`。 | 游戏内人工验证与真实存档 round-trip 演练待补。 |

## 3. 自动化证据

- 全量命令：`QT_QPA_PLATFORM=offscreen PYTHONPATH=src pytest tests -q`
- 当前结果：`27 passed`
- 关键模块：
  - `src/ac6_data_manager/container_workspace/*`
  - `src/ac6_data_manager/emblem/*`
  - `src/ac6_data_manager/validation/*`
  - `src/ac6_data_manager/gui/*`
  - `src/ac6_data_manager/app.py`

## 4. AC Gate 复核结论

| Gate | 当前状态 | 锁定原因 |
|---|---|---|
| `record map proven` | `LOCKED` | `docs/reverse/ac-record-map.md` 与 `artifacts/ac-record-diff-corpus/` 仍缺。 |
| `dependency graph proven` | `LOCKED` | `docs/reverse/ac-dependency-graph.md` 与 `artifacts/ac-dependency-traces/` 仍缺。 |
| `editability transition proven` | `LOCKED` | `docs/reverse/ac-editability-transition.md` 与对应实验语料仍缺。 |
| `preview provenance proven` | `LOCKED` | `docs/reverse/ac-preview-provenance.md` 与 `artifacts/ac-preview-traces/` 仍缺。 |
| `round-trip corpus proven` | `LOCKED` | AC lane 对应 round-trip corpus、删除/恢复实验、实机验证仍缺。 |

## 5. 建议交接项

1. 用真实 WitchyBND + 真实 `.sl2` 做 emblem stable lane 外部演练，并补齐 round-trip / rollback / incident 证据。
2. 在游戏内完成人工验收：可见、可编辑、可保存、预览正确。
3. 按 `docs/appendices/ac6-data-manager.m2-readiness.md` 建立 `docs/reverse/` 与 AC read-only explorer 所需证据。
4. 在 AC gate 未闭合前，继续禁止任何 AC mutation 宣称或入口。
