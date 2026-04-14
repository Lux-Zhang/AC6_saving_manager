# -*- mode: python ; coding: utf-8 -*-

from pathlib import Path

from PyInstaller.utils.hooks import collect_submodules

project_root = Path(SPEC).resolve().parents[1]
entry_script = project_root / "src" / "ac6_data_manager" / "app.py"
manifest_path = project_root / "packaging" / "assets" / "app.manifest"

hiddenimports = collect_submodules("PyQt5")

a = Analysis(
    [str(entry_script)],
    pathex=[str(project_root), str(project_root / "src")],
    binaries=[],
    datas=[],
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="AC6 saving manager",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    manifest=str(manifest_path),
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=False,
    name="AC6 saving manager",
)
