<div align="center">

# AC6 Saving Manager

**A native Windows toolkit for browsing, importing, exporting, and qualifying ARMORED CORE VI save assets.**

[![Version](https://img.shields.io/badge/version-v0.0.1-2f81f7)](https://github.com/Lux-Zhang/AC6_saving_manager/releases/tag/v0.0.1)
[![License: MIT](https://img.shields.io/badge/license-MIT-ffd43b.svg)](LICENSE)
[![Windows CI](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4)](#build-from-source)
[![Qt](https://img.shields.io/badge/Qt-6.6.3-41cd52)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599c)](https://isocpp.org/)

[Highlights](#highlights) · [Build](#build-from-source) · [Release Flow](#release-flow) · [Project Layout](#project-layout) · [Docs](#documentation)

**English** · [中文](docs/lang/README_ZH.md) · [Русский](docs/lang/README_RU.md) · [日本語](docs/lang/README_JA.md)

</div>

---

## Highlights

- Native Qt 6 desktop application for real `.sl2` save inspection on Windows x64
- Two-library workflow for **Emblems** and **ACs**
- Share-to-user import workflow with explicit target slot selection
- Record-level AC exchange packages and emblem exchange packages
- Portable one-folder release packaging with `WitchyBND` sidecar manifest gating
- GitHub Actions Windows CI plus gated release automation

## Current Scope

This public baseline is **v0.0.1**.

The current delivery lane focuses on:

- Stable Windows desktop packaging
- Native save open/browse/import/export workflows
- CI/CD hardening for portable release delivery

The release workflow is intentionally **gated**. By default, the repository only runs CI. Automated GitHub Releases stay disabled until `ENABLE_RELEASE=true`.

## Build From Source

### Prerequisites

- Windows x64
- Visual Studio 2022 build tools
- Qt 6.6.x desktop MSVC package
- CMake 3.24+
- OpenSSL and ZLIB available to CMake

### Configure and build

```powershell
powershell -File scripts/build_native_windows.ps1 -BuildDir build/native -Config Release
```

### Run tests

```powershell
ctest --test-dir build/native -C Release --output-on-failure
```

### Package a local portable build

Local packaging still expects a resolved `WitchyBND` directory:

```powershell
powershell -File scripts/build_windows_portable.ps1 `
  -ThirdPartySource <path-to-WitchyBND-directory> `
  -ReleaseDir Release `
  -BuildDir build/native `
  -Config Release
```

## Release Flow

### CI

`windows-ci.yml` runs on:

- pushes to `main`
- pushes to `codex/**`
- pushes to `wip/**`
- pull requests targeting `main`
- manual dispatch

It performs:

1. Qt + MSVC environment setup
2. vcpkg dependency installation
3. native build
4. native test execution
5. install tree verification
6. artifact upload

### Release

`release-windows.yml` is designed for `main + tag`, but is disabled by default.

When you eventually enable automated releases:

1. set repository variable `ENABLE_RELEASE=true`
2. push a tag such as `v0.0.1`
3. the workflow resolves the **latest** `ividyon/WitchyBND` Windows release asset dynamically
4. the workflow validates the upstream `sha256:` digest before packaging
5. the workflow publishes a GitHub Release with the portable bundle and evidence files

## Project Layout

```text
native/       Qt 6 + C++20 application and native tests
scripts/      Windows build, packaging, smoke, and acceptance scripts
packaging/    portable release assets and sidecar manifest templates
docs/         PRD, technical design, appendices, and language docs
.github/      GitHub Actions workflows and shared CI actions
```

## Documentation

- [Chinese README](docs/lang/README_ZH.md)
- [Russian README](docs/lang/README_RU.md)
- [Japanese README](docs/lang/README_JA.md)
- [GitHub CI/CD Notes](docs/appendices/ac6-data-manager.github-cicd.md)
- [M1.5 Release Runbook](docs/appendices/ac6-data-manager.m1_5-release-runbook.md)

## License

MIT License © Lux-Zhang
