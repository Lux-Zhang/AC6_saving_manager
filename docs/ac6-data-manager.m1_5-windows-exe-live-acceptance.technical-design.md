# AC6 Data Manager M1.5 Technical Design

- 文档状态：Approved for Execution
- 版本：v0.9-m1.5
- 最后更新：2026-04-14
- 关联 PRD：`ac6-data-manager.m1_5-windows-exe-live-acceptance.prd.md`
- 关联附录：
  - `appendices/ac6-data-manager.m1_5-live-acceptance-checklist.md`
  - `appendices/ac6-data-manager.m1_5-release-runbook.md`
  - `appendices/ac6-data-manager.m1_5-huorong-risk-notes.md`
  - `appendices/ac6-data-manager.m1_5-third-party-manifest-spec.md`
  - `appendices/ac6-data-manager.m1_5-evidence-index.md`

## 1. 技术目标
M1.5 的技术目标不是增加新功能面，而是把当前已有的 M0.5 / M1 基线，升级为一个可以在 Windows x64 上：
- 独立启动
- 离线运行
- 对 third-party sidecar 有硬治理
- 对发布与回滚有硬契约
- 对人工验收有完整证据
的 portable release。

## 2. 当前代码基线
### 2.1 已有模块
- 容器工作区：`src/ac6_data_manager/container_workspace/`
- 徽章 stable lane：`src/ac6_data_manager/emblem/`
- GUI 壳层：`src/ac6_data_manager/gui/`
- 验证与回滚：`src/ac6_data_manager/validation/`
- GUI 入口：`src/ac6_data_manager/app.py`

### 2.2 当前关键技术缺口
1. `src/ac6_data_manager/app.py` 当前默认构建 demo provider，不可直接作为 release 入口。
2. `src/ac6_data_manager/container_workspace/workspace.py` 当前 publish 语义未完全对齐 rollback 的 fresh destination only 规则。
3. `src/ac6_data_manager/container_workspace/adapter.py` 目前主要针对 `WitchyBND.exe` 做 hash / version 校验，尚未覆盖目录级 manifest gate。
4. 当前仓内自动化验证通过，但 Windows exe、portable release、Huorong、真实人工验收仍缺正式闭环。

## 3. 设计原则
1. 单主路径：Builder 只允许 PyInstaller one-folder。
2. 真实入口：release 默认入口必须走真实 stable-emblem service bundle。
3. 硬阻断：third-party、publish、rollback、readback 失败一律 fail-closed。
4. 证据优先：所有关键步骤都要生成可索引证据。
5. 环境可复验：任何 Windows / Huorong 结果都必须可被记录和追溯。

## 4. 目标目录契约
### 4.1 仓库内新增结构
```text
packaging/
  pyinstaller.spec
  assets/
    app.manifest
    icon_prompt.md
  third_party/
    manifest.template.json
scripts/
  build_windows_portable.ps1
  build_windows_portable.py
  verify_third_party_manifest.py
  run_preflight_check.py
  create_release_zip.py
  smoke_test_portable.ps1
  live_acceptance_capture.ps1
artifacts/
  release/
  live_acceptance/
  preflight/
  incident/
  huorong/
```

### 4.2 release 目录契约
```text
release/
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

## 5. Builder 设计
### 5.1 主路径
- 固定：PyInstaller one-folder
- 目标：稳定、可诊断、便于 sidecar 分发
- 原因：当前需求是 Windows 可交付与人工验收，不是压缩到极限体积或单文件体验

### 5.2 Contingency
- `Nuitka` 不进入默认执行路径
- 仅在 PyInstaller 路径出现可复现 blocker 且超出排障预算时才允许切换
- 切换前必须先记录 blocker 与失败证据

## 6. Runtime / Entry 设计
### 6.1 默认入口模式
release 默认入口必须：
- 创建真实 `ServiceBundleGuiDataProvider`
- 接入 stable-emblem 实际服务链路
- 输出 provider 身份与运行模式日志

### 6.2 Demo 模式
`FixtureGuiDataProvider` 仅允许：
- `--demo`
- 开发测试模式
- GUI fixture 测试

release 包内默认流程不允许回退到 demo provider。

### 6.3 真实入口证明清单
至少输出以下证据：
- 启动日志中的 provider 类型
- 当前运行模式不是 demo
- preflight 结果
- bundled WitchyBND 实际解析路径
- stable-emblem 服务链路初始化结果

## 7. Third-party Sidecar 设计
### 7.1 基线来源
- 参考基线：`../ACVIEmblemCreator/` bundled version
- 需要以目录整体作为 reference source，而不是只对比 `WitchyBND.exe`

### 7.2 目录级 manifest gate
需要对 `third_party/WitchyBND/**` 输出 manifest，包含：
- 相对路径
- sha256
- required
- source_baseline
- version_label
- file_role

### 7.3 校验结果
以下任一结果直接 fail-closed：
- required 文件缺失
- extra 文件出现
- sha256 mismatch
- version mismatch
- entrypoint 不存在
- 解析到系统 PATH 中的外部 WitchyBND

## 8. Preflight 设计
preflight 至少包含：
1. OS / arch 检查
2. release 目录完整性检查
3. third-party manifest gate
4. 入口模式检查
5. 写出目录权限检查
6. save 输入路径与输出路径策略检查
7. offline readiness 检查

preflight 结果必须同时输出：
- machine-readable json
- 用户可读文本摘要

## 9. Controlled Publish / Rollback 设计
### 9.1 Publish
- 输入：verified shadow output
- 输出：fresh destination only
- 约束：目标路径存在即拒绝
- 审计：输出路径、时间、hash、readback 结果、operator note

### 9.2 Rollback
- 输入：verified restore-point backup
- 输出：fresh destination only
- 约束：不可写回 source save，也不可覆盖已存在目标
- 审计：rollback source、rollback target、hash 验证、结果

### 9.3 Readback Gate
任何 publish 成功声明必须建立在 post-write readback 成功之上；readback mismatch 直接阻断后续 publish verdict。

## 10. Huorong 验收矩阵
Huorong 不再作为“附带说明”，而是正式测试轴。必须记录以下步骤：
1. 解压 portable zip
2. 首次启动 exe
3. preflight
4. sidecar 调用
5. dry-run
6. apply 到 fresh output
7. rollback
8. 二次启动

每一步均标记：
- pass
- manual-allow
- blocked

并明确该步结果是否会改变 release verdict。

## 11. 审计证据包
证据包最小 manifest 至少包含：
- build metadata
- release content manifest
- third-party gate result
- preflight result
- real-entry proof
- Huorong matrix
- publish audit
- rollback readback evidence
- smoke / live acceptance summary

## 12. 测试与验证
### 12.1 仓内验证
- `pytest`
- affected-file diagnostics
- provider mode tests
- manifest gate tests
- publish/rollback fresh destination tests

### 12.2 Windows live acceptance
- portable zip 解压
- exe 首次启动
- preflight
- real stable-emblem entry
- manual share emblem import path
- post-write readback
- rollback drill
- in-game verify

## 13. 实施分工建议
### Worker1
- PyInstaller one-folder
- release layout
- smoke baseline

### Worker2
- WitchyBND directory manifest gate
- baseline alignment
- preflight / no-PATH-fallback

### Worker3
- controlled publish / rollback
- readback / audit / incident artifact
- live acceptance harness

### Worker4
- checklist / runbook / Huorong / evidence index / verdict truth table

### Verifier
- `ZPX-GE77 + Windows 10 25H2 + 火绒开启` 条件下复核全链路证据

## 14. 已知技术风险
1. PyInstaller + PyQt5 打包不稳定
2. sidecar 目录漂移
3. 默认入口误落回 demo provider
4. fresh destination only 语义未完全落实
5. 火绒误报 / 删除 / 隔离
6. 当前环境无法直接完成真实 Windows 真机验收时，可能只能交付 evidence-ready 状态
