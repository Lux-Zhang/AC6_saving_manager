# AC6 Saving Manager

**面向 ARMORED CORE VI 存档资产浏览、导入、导出与发布校验的原生 Windows 工具。**

[![Version](https://img.shields.io/badge/version-v0.0.1-2f81f7)](https://github.com/Lux-Zhang/AC6_saving_manager/releases/tag/v0.0.1)
[![License: MIT](https://img.shields.io/badge/license-MIT-ffd43b.svg)](../../LICENSE)
[![Windows CI](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4)](../../README.md#build-from-source)
[![Qt](https://img.shields.io/badge/Qt-6.6.3-41cd52)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599c)](https://isocpp.org/)

[English](../../README.md) · [亮点](#亮点) · [构建](#本地构建) · [发布流](#发布流) · [文档](#文档)

[English](../../README.md) · **中文** · [Русский](README_RU.md) · [日本語](README_JA.md)

---

## 亮点

- 基于 Qt 6 的原生 Windows x64 桌面程序，可直接打开真实 `.sl2` 存档
- `Emblems` / `ACs` 双库工作流
- 共享资产导入到使用者栏位时，强制显式选择目标槽位
- 支持徽章交换包与 record-level AC 交换包
- 支持 one-folder portable 发布，并通过 `WitchyBND` sidecar manifest gate 做约束
- 已接入 GitHub Actions Windows CI 和受控发布流

## 当前范围

当前公开基线版本为 **v0.0.1**。

本阶段重点是：

- Windows 原生桌面发布
- 存档打开 / 浏览 / 导入 / 导出主流程
- CI/CD 与 portable release 的稳定化

自动 GitHub Release 默认仍然关闭。仓库当前只跑 CI；只有在显式把 `ENABLE_RELEASE=true` 之后，才会真正开放自动发版。

## 本地构建

### 前置条件

- Windows x64
- Visual Studio 2022 build tools
- Qt 6.6.x desktop MSVC 包
- CMake 3.24+
- CMake 可解析的 OpenSSL 与 ZLIB

### 编译

```powershell
powershell -File scripts/build_native_windows.ps1 -BuildDir build/native -Config Release
```

### 测试

```powershell
ctest --test-dir build/native -C Release --output-on-failure
```

### 本地打 portable 包

本地打包仍然需要你提供一个已经解析好的 `WitchyBND` 目录：

```powershell
powershell -File scripts/build_windows_portable.ps1 `
  -ThirdPartySource <WitchyBND目录> `
  -ReleaseDir Release `
  -BuildDir build/native `
  -Config Release
```

## 发布流

### CI

`windows-ci.yml` 会在以下场景运行：

- push 到 `main`
- push 到 `codex/**`
- push 到 `wip/**`
- PR 到 `main`
- 手动触发

执行内容固定为：

1. Qt + MSVC 环境准备
2. vcpkg 安装依赖
3. 原生构建
4. 原生测试
5. install tree 校验
6. artifact 上传

### Release

`release-windows.yml` 设计为 `main + tag` 发布，但默认关闭。

当你之后真正开放自动发版时：

1. 把仓库变量 `ENABLE_RELEASE` 改成 `true`
2. 推送一个 tag，例如 `v0.0.1`
3. workflow 会动态读取 `ividyon/WitchyBND` 的**最新** Windows release 资产
4. workflow 会对 upstream 提供的 `sha256:` digest 做校验
5. 校验通过后才会打包并发布 GitHub Release

## 项目结构

```text
native/       Qt 6 + C++20 应用和 native tests
scripts/      Windows 构建、打包、smoke、验收脚本
packaging/    portable 发布资产与 sidecar manifest 模板
docs/         PRD、技术设计、附录与语言文档
.github/      GitHub Actions workflows 和共享 CI action
```

## 文档

- [English README](../../README.md)
- [Русский README](README_RU.md)
- [日本語 README](README_JA.md)
- [GitHub CI/CD 说明](../appendices/ac6-data-manager.github-cicd.md)
- [M1.5 发布 Runbook](../appendices/ac6-data-manager.m1_5-release-runbook.md)

## 协议

MIT License © Lux-Zhang
