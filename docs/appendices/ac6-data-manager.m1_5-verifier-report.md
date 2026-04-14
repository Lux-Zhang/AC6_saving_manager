# AC6 Data Manager M1.5 Verifier Report（Windows EXE Packaging & Live Acceptance）

- 状态：已根据当前仓库实现刷新
- 日期：2026-04-14
- 适用范围：仅复核 `M1.5` Windows x64 one-folder portable release 与 live acceptance evidence baseline
- 当前约束：继续禁止 `AC mutation`；五个 `AC gate` 继续保持 `LOCKED`
- 复核方式：仓库代码、release 相关自动化测试、PRD / technical design / appendices 一致性复核；本次未运行 `ZPX-GE77` 真机、未伪造 `Huorong` 或游戏内 evidence

## 1. 结论摘要

1. `portable release / runtime / sidecar / publish / rollback / evidence manifest`：**REPO-PASS**。仓内实现与自动化测试已经覆盖默认入口、manifest gate、fresh-destination-only、rollback evidence 与 verdict 计算。
2. `M1.5` 当前总体判定：**RELEASE-READY / LIVE-PENDING**。可以声明仓内发布链路和证据契约已具备，但不能在当前会话中提升到 `live-pass`。
3. `live-pass` 仍依赖 `ZPX-GE77 + Windows 10 25H2 + Huorong enabled` 的真实 unzip、首启、preflight、sidecar、dry-run、apply、rollback、二次启动与游戏内复核证据。
4. `AC gate`：5 个 gate 继续保持 `LOCKED`，因此本轮不解锁任何 AC mutation / experimental import 入口。

## 2. M1.5 复核表

| Capability | 当前结论 | 正向证据 | 阻塞项 |
|---|---|---|---|
| portable release layout / unzip baseline | `REPO-PASS` | `tests/test_release_build.py`；`src/ac6_data_manager/release/build.py`；`docs/appendices/ac6-data-manager.m1_5-release-runbook.md` | 未在 `ZPX-GE77` 上做真实 unzip/launch 记录。 |
| 默认入口 / first launch runtime | `REPO-PASS` | `tests/test_release_runtime.py`；`src/ac6_data_manager/release/runtime.py`；`artifacts/preflight/real-entry-proof.json` 位点已定义 | 真机首启与 `Huorong` 实际拦截结果待补。 |
| sidecar manifest gate / preflight | `REPO-PASS` | `tests/test_release_third_party.py`、`tests/test_release_preflight.py`；`src/ac6_data_manager/release/third_party.py`、`src/ac6_data_manager/release/preflight.py` | 实机 `third_party/WitchyBND` baseline 与 `Huorong` 行为待补。 |
| dry-run / apply / readback / rollback evidence | `REPO-PASS` | `tests/test_release_publish.py`；`src/ac6_data_manager/release/publish.py`；`docs/appendices/ac6-data-manager.m1_5-evidence-index.md` | 真实 `.sl2` 与外部工具 end-to-end 演练待补。 |
| evidence manifest / release verdict | `REPO-PASS` | `tests/test_release_evidence.py`；`src/ac6_data_manager/release/evidence.py`；`docs/ac6-data-manager.m1_5-windows-exe-live-acceptance.prd.md` | `Huorong` 矩阵、checklist capture、游戏内 verify 仍缺真机证据。 |
| ZPX-GE77 live acceptance | `LIVE-PENDING` | `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md` 已明确待执行项与当前 verdict | 当前会话不具备目标机器 / 安全软件 / 游戏内验证环境。 |

## 3. 自动化证据

- 复核命令：
  - `PYTHONPATH=src python -m pytest tests/test_release_build.py tests/test_release_runtime.py tests/test_release_third_party.py tests/test_release_preflight.py tests/test_release_publish.py tests/test_release_evidence.py -q`
- 当前结果：`17 passed`
- 关注模块：
  - `src/ac6_data_manager/release/build.py`
  - `src/ac6_data_manager/release/runtime.py`
  - `src/ac6_data_manager/release/third_party.py`
  - `src/ac6_data_manager/release/preflight.py`
  - `src/ac6_data_manager/release/publish.py`
  - `src/ac6_data_manager/release/evidence.py`

## 4. Live-pass 阻塞条件

只有在以下条件全部补齐后，`M1.5` 才能从 `RELEASE-READY / LIVE-PENDING` 提升为 `live-pass`：

1. `ZPX-GE77` 上完成 unzip、首启、preflight、sidecar、dry-run、apply、rollback、二次启动；
2. `Huorong` 每一步都有 `pass / manual-allow / blocked` 记录；
3. 游戏内 `visible / editable / saveable` 复核完成；
4. 证据文件落到 `artifacts/live_acceptance/`、`artifacts/huorong/` 与 `docs/appendices/ac6-data-manager.m1_5-zpx-ge77-live-acceptance-record.md`。

## 5. 建议交接项

1. 在目标真机上按 `docs/appendices/ac6-data-manager.m1_5-live-acceptance-checklist.md` 与 `docs/appendices/ac6-data-manager.m1_5-release-runbook.md` 完整执行 live acceptance。
2. 将 `Huorong` 每一步的结果沉淀为结构化矩阵，并与 `derive_release_verdict()` 的 `fail / release-pending / conditional-pass / live-pass` 分支对齐。
3. 若任一步出现 `blocked` 或 readback mismatch，继续保持 `fail-closed`，并把 incident / rollback 证据归档到 `artifacts/live_acceptance/`。
