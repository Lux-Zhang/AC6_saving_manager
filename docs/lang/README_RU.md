# AC6 Saving Manager

**Нативный инструмент для Windows, предназначенный для просмотра, импорта, экспорта и квалификации ресурсов сохранений ARMORED CORE VI.**

[![Version](https://img.shields.io/badge/version-v0.0.1-2f81f7)](https://github.com/Lux-Zhang/AC6_saving_manager/releases/tag/v0.0.1)
[![License: MIT](https://img.shields.io/badge/license-MIT-ffd43b.svg)](../../LICENSE)
[![Windows CI](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/Lux-Zhang/AC6_saving_manager/actions/workflows/windows-ci.yml)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4)](../../README.md#build-from-source)
[![Qt](https://img.shields.io/badge/Qt-6.6.3-41cd52)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599c)](https://isocpp.org/)

[English](../../README.md) · [中文](README_ZH.md) · **Русский** · [日本語](README_JA.md)

---

## Основные возможности

- Нативное Qt 6-приложение для Windows x64, открывающее реальные `.sl2` сохранения
- Двухбиблиотечный рабочий поток для **Emblems** и **ACs**
- Импорт ресурсов из share-источника в пользовательский слот с явным выбором целевого слота
- Поддержка exchange-пакетов эмблем и AC на уровне записей
- Portable one-folder выпуск с manifest gate для `WitchyBND` sidecar
- Windows CI в GitHub Actions и контролируемый release pipeline

## Текущее покрытие

Текущая публичная базовая версия: **v0.0.1**.

На этом этапе проект сосредоточен на:

- стабильном нативном выпуске для Windows
- открытии, просмотре, импорте и экспорте сохранений
- усилении CI/CD и portable release delivery

Автоматический GitHub Release по умолчанию **отключен**. Сейчас репозиторий выполняет только CI. Полноценный автоматический выпуск включается только после установки `ENABLE_RELEASE=true`.

## Локальная сборка

### Требования

- Windows x64
- build tools из Visual Studio 2022
- Qt 6.6.x desktop MSVC package
- CMake 3.24+
- OpenSSL и ZLIB, доступные для `find_package`

### Сборка

```powershell
powershell -File scripts/build_native_windows.ps1 -BuildDir build/native -Config Release
```

### Тесты

```powershell
ctest --test-dir build/native -C Release --output-on-failure
```

### Локальная упаковка portable-сборки

Для локальной упаковки по-прежнему нужен заранее подготовленный каталог `WitchyBND`:

```powershell
powershell -File scripts/build_windows_portable.ps1 `
  -ThirdPartySource <путь-к-каталогу-WitchyBND> `
  -ReleaseDir Release `
  -BuildDir build/native `
  -Config Release
```

## Release pipeline

### CI

`windows-ci.yml` запускается при:

- push в `main`
- push в `codex/**`
- push в `wip/**`
- pull request в `main`
- ручном запуске

Выполняемые шаги:

1. подготовка Qt + MSVC среды
2. установка зависимостей через vcpkg
3. нативная сборка
4. запуск нативных тестов
5. проверка install tree
6. загрузка artifact

### Release

`release-windows.yml` рассчитан на схему `main + tag`, но по умолчанию выключен.

Когда вы позже включите автоматические выпуски:

1. установите `ENABLE_RELEASE=true`
2. отправьте tag, например `v0.0.1`
3. workflow динамически определит **последний** Windows release asset из `ividyon/WitchyBND`
4. workflow проверит upstream `sha256:` digest перед упаковкой
5. workflow опубликует GitHub Release с portable bundle и evidence-файлами

## Структура проекта

```text
native/       Qt 6 + C++20 приложение и native tests
scripts/      Windows-скрипты сборки, упаковки, smoke и acceptance
packaging/    ресурсы portable release и шаблоны sidecar manifest
docs/         PRD, технический дизайн, приложения и языковые страницы
.github/      GitHub Actions workflows и общий CI action
```

## Документация

- [English README](../../README.md)
- [中文 README](README_ZH.md)
- [日本語 README](README_JA.md)
- [GitHub CI/CD Notes](../appendices/ac6-data-manager.github-cicd.md)
- [M1.5 Release Runbook](../appendices/ac6-data-manager.m1_5-release-runbook.md)

## Лицензия

MIT License © Lux-Zhang
