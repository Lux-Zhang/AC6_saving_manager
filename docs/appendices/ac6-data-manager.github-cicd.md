# AC6_saving_manager GitHub CI/CD 说明

## 1. 目标

本文定义仓库内 GitHub Actions 的最小长期方案：

- `windows-ci.yml` 负责 Windows x64 构建、测试、install tree 校验
- `release-windows.yml` 负责基于 `main + tag` 的正式打包与 GitHub Release 发布
- `WitchyBND` sidecar 不入仓，长期从外部下载

当前项目继续保持 Windows-only，不为 Linux 或 macOS 构造伪支持矩阵。

## 2. Workflow 清单

### 2.1 `windows-ci.yml`

触发条件：

- `push` 到 `main`
- `push` 到 `codex/**`
- `push` 到 `wip/**`
- `pull_request` 到 `main`
- `workflow_dispatch`

核心步骤：

1. checkout
2. 初始化 MSVC x64 环境
3. 安装 Qt 6.6.3 x64
4. 安装 vcpkg，并拉起 `openssl:x64-windows` 与 `zlib:x64-windows`
5. 通过 `scripts/build_native_windows.ps1` 构建
6. 执行 `ctest --test-dir build/native -C Release --output-on-failure`
7. 执行 `cmake --install build/native --config Release --prefix artifacts/install-root`
8. 上传 install tree 与测试日志

必过检查名固定为：

- `windows-build-test`

### 2.2 `release-windows.yml`

触发条件：

- `push` tag `v*`
- `workflow_dispatch`

工作流分为三段：

1. `release-gate`
   - 读取仓库变量
   - 决定是否允许正式 release
   - 输出 sidecar 配置状态和 release tag
2. `release-preflight`
   - 仅在 `ENABLE_RELEASE=true` 时运行
   - 复用 CI 的 build/test/install 路径
   - 校验 `WITCHYBND_DOWNLOAD_URL`
3. `release-package-publish`
   - 仅在 release enabled、preflight 通过且存在 release tag 时运行
   - 下载并校验 WitchyBND sidecar
   - 调用 `scripts/package_native_windows.ps1`
   - 调用 `scripts/smoke_test_portable.ps1`
   - 生成 zip 并上传到 GitHub Release

## 3. 必要远端仓库设置

以下设置不在仓库文件内表达，必须在 GitHub 仓库设置页面完成：

1. 将默认分支切回 `main`
2. 仅对 `main` 启用 branch protection
3. 将 `windows-build-test` 设为 required status check

建议保持：

- `codex/**` 和 `wip/**` 不加必过保护，供开发分支直接迭代

## 4. Repository Variables

`release-windows.yml` 依赖以下仓库变量：

- `ENABLE_RELEASE`
  - 默认值：`false`
  - 只有显式改为 `true` 才允许正式发版
- `WITCHYBND_DOWNLOAD_URL`
  - 指向 WitchyBND sidecar 压缩包
  - 当前 workflow 期望下载物为 zip
  - zip 可以是：
    - 根目录直接包含 `WitchyBND.exe`
    - 或仅有一个顶层目录，该目录内包含 `WitchyBND.exe`
- `WITCHYBND_SHA256`
  - 可选
  - 若配置，workflow 必须校验下载包 SHA256 完全一致
  - 若不配置，workflow 仍会记录 provenance，但不会伪造校验通过

## 5. 手动触发 release

`workflow_dispatch` 提供可选输入：

- `release_tag`

用法：

- tag push 触发时，workflow 自动使用当前 tag
- 手动触发时，如要真正创建 GitHub Release，必须提供 `release_tag`
- 若手动触发但未提供 `release_tag`，workflow 只会跑 gate 或 preflight，不会发布 release

## 6. 验收标准

### 6.1 CI

通过条件：

- `windows-build-test` 为绿色
- artifact 内包含：
  - `artifacts/install-root`
  - `artifacts/ci/logs`
  - `build/native/Testing`

### 6.2 Release

通过条件：

- `ENABLE_RELEASE=false` 时，workflow summary 明确提示 `release disabled`
- `ENABLE_RELEASE=true` 且 sidecar URL 可用时：
  - preflight 成功
  - `scripts/package_native_windows.ps1` 成功
  - `scripts/smoke_test_portable.ps1` 成功
  - GitHub Release 附件包含：
    - Windows portable zip
    - `smoke-baseline.json`
    - `third_party_manifest.json`

## 7. 当前默认

- 当前阶段默认只开启 CI
- 默认不自动对 tag 发布产物，除非显式打开 `ENABLE_RELEASE`
- `WitchyBND` sidecar 的长期策略是外部下载与独立更新，不进入仓库版本控制
