# AC6 Data Manager M1.5 Technical Design

- 文档状态：Approved for Execution
- 版本：v1.0-m1.5
- 最后更新：2026-04-14

## 1. 实现对象与范围
本轮只补齐 M1.5 收口能力，不扩展 M2/M3：
- `src/ac6_data_manager/release/runtime.py`：真实默认入口、provider proof、demo/dev gating
- `src/ac6_data_manager/release/third_party.py`：WitchyBND 目录级 manifest 生成与校验
- `src/ac6_data_manager/release/preflight.py`：OS/arch、release layout、manifest gate、runtime entry、publish contract、offline readiness 汇总
- `src/ac6_data_manager/release/publish.py`：publish/readback/rollback evidence 输出
- `src/ac6_data_manager/release/build.py`：portable release layout 与 staging
- `src/ac6_data_manager/release/evidence.py`：release content manifest、evidence manifest、verdict 计算
- `src/ac6_data_manager/app.py`：默认入口接入真实 service bundle；`--demo` / `--provider-proof-json` / `--headless-provider-check`
- `src/ac6_data_manager/container_workspace/workspace.py`：publish 输出路径改为 fresh-destination-only

## 2. 默认入口设计
- 默认模式：`ServiceBundleGuiDataProvider`
- demo 仅限：`--demo` 或开发模式环境变量
- provider proof 通过 `--provider-proof-json` 或 `--headless-provider-check` 输出 JSON
- proof 必须包含：`provider_kind`、`runtime_mode`、`is_demo`、`toolchain_policy`

## 3. Sidecar Manifest Gate
- manifest 位置：`third_party/third_party_manifest.json`
- entrypoint：`third_party/WitchyBND/WitchyBND.exe`
- 校验维度：required file、extra file、sha256、version label、entrypoint presence
- 策略：任一失败都 `fail_closed`

## 4. Portable Release Layout
```text
artifacts/release/
  AC6 saving manager/
    AC6 saving manager.exe
    runtime/
    resources/
    third_party/
      WitchyBND/
      third_party_manifest.json
    docs/
      QUICK_START.txt
      LIVE_ACCEPTANCE_CHECKLIST.txt
      KNOWN_LIMITATIONS.txt
```

## 5. Publish / Rollback Contract
- publish：只允许写到 fresh destination；目标已存在立即阻断
- rollback：继续只允许 fresh destination
- 读回：publish 结果必须附带 readback report
- incident：失败时复制 incident artifact 到 evidence 目录

## 6. 验证
- `tests/test_release_runtime.py`
- `tests/test_release_third_party.py`
- `tests/test_release_preflight.py`
- `tests/test_release_build.py`
- `tests/test_release_publish.py`
- `tests/test_container_workspace.py`

当前仓内自动化状态：`40 passed`
