# AC6 Saving Manager

**ARMORED CORE VI のセーブ資産を閲覧・インポート・エクスポート・検証するためのネイティブ Windows ツールです。**

[![Version](https://img.shields.io/badge/version-v0.0.1-2f81f7)](https://github.com/Lux-Zhang/AC6_saving_manager/releases/tag/v0.0.1)
[![License: MIT](https://img.shields.io/badge/license-MIT-ffd43b.svg)](../../LICENSE)
[![Windows CI](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4)](../../README.md#build-from-source)
[![Qt](https://img.shields.io/badge/Qt-6.6.3-41cd52)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599c)](https://isocpp.org/)

[English](../../README.md) · [中文](README_ZH.md) · [Русский](README_RU.md) · **日本語**

---

## 特徴

- 実際の `.sl2` セーブを開ける Windows x64 向け Qt 6 ネイティブアプリ
- **Emblems** と **ACs** の 2 ライブラリ構成
- Share 資産をユーザー欄へ取り込む際に、対象スロットを明示的に選択
- エンブレム交換パッケージと record-level AC 交換パッケージに対応
- `WitchyBND` sidecar manifest gate を用いた one-folder portable リリース
- GitHub Actions の Windows CI と制御付き release フロー

## 現在のスコープ

現在の公開ベースラインは **v0.0.1** です。

この段階の重点は次の通りです。

- Windows ネイティブ配布の安定化
- セーブのオープン、閲覧、インポート、エクスポート
- portable release delivery に向けた CI/CD の強化

自動 GitHub Release はデフォルトで**無効**です。現在のリポジトリは CI のみを実行し、`ENABLE_RELEASE=true` を明示したときだけ自動リリースを有効にします。

## ローカルビルド

### 前提条件

- Windows x64
- Visual Studio 2022 build tools
- Qt 6.6.x desktop MSVC package
- CMake 3.24+
- `find_package` から参照できる OpenSSL と ZLIB

### ビルド

```powershell
powershell -File scripts/build_native_windows.ps1 -BuildDir build/native -Config Release
```

### テスト

```powershell
ctest --test-dir build/native -C Release --output-on-failure
```

### ローカルで portable パッケージを作る

ローカルのパッケージ作成では、解決済みの `WitchyBND` ディレクトリが必要です。

```powershell
powershell -File scripts/build_windows_portable.ps1 `
  -ThirdPartySource <WitchyBNDディレクトリへのパス> `
  -ReleaseDir Release `
  -BuildDir build/native `
  -Config Release
```

## リリースフロー

### CI

`windows-ci.yml` は以下で実行されます。

- `main` への push
- `codex/**` への push
- `wip/**` への push
- `main` 向け pull request
- 手動実行

実行内容：

1. Qt + MSVC 環境の準備
2. vcpkg による依存関係のインストール
3. ネイティブビルド
4. ネイティブテスト実行
5. install tree 検証
6. artifact アップロード

### Release

`release-windows.yml` は `main + tag` を前提に設計されていますが、初期状態では無効です。

将来自動リリースを有効にする場合は次の手順です。

1. `ENABLE_RELEASE=true` を設定
2. `v0.0.1` のような tag を push
3. workflow が `ividyon/WitchyBND` の**最新** Windows release asset を動的に解決
4. workflow が upstream の `sha256:` digest を検証
5. workflow が portable bundle と証跡ファイル付きで GitHub Release を公開

## プロジェクト構成

```text
native/       Qt 6 + C++20 アプリケーションと native tests
scripts/      Windows 用のビルド、パッケージ、smoke、acceptance スクリプト
packaging/    portable release 資産と sidecar manifest テンプレート
docs/         PRD、技術設計、付録、各言語ドキュメント
.github/      GitHub Actions workflows と共有 CI action
```

## ドキュメント

- [English README](../../README.md)
- [中文 README](README_ZH.md)
- [Русский README](README_RU.md)
- [GitHub CI/CD Notes](../appendices/ac6-data-manager.github-cicd.md)
- [M1.5 Release Runbook](../appendices/ac6-data-manager.m1_5-release-runbook.md)

## ライセンス

MIT License © Lux-Zhang
