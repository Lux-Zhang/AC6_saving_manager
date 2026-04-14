from __future__ import annotations

import shutil
from dataclasses import dataclass
from pathlib import Path

from .third_party import ThirdPartyManifest, build_manifest_for_release

APP_DIRNAME = "AC6 saving manager"
EXE_NAME = "AC6 saving manager.exe"
DOC_ASSET_NAMES = (
    "QUICK_START.txt",
    "LIVE_ACCEPTANCE_CHECKLIST.txt",
    "KNOWN_LIMITATIONS.txt",
)


@dataclass(frozen=True)
class PortableReleaseLayout:
    base_dir: Path
    app_root: Path
    exe_path: Path
    runtime_root: Path
    resources_root: Path
    third_party_root: Path
    witchybnd_root: Path
    manifest_path: Path
    docs_root: Path


def build_release_layout(base_dir: Path) -> PortableReleaseLayout:
    base_dir = Path(base_dir)
    app_root = base_dir / APP_DIRNAME
    return PortableReleaseLayout(
        base_dir=base_dir,
        app_root=app_root,
        exe_path=app_root / EXE_NAME,
        runtime_root=app_root / "runtime",
        resources_root=app_root / "resources",
        third_party_root=app_root / "third_party",
        witchybnd_root=app_root / "third_party" / "WitchyBND",
        manifest_path=app_root / "third_party" / "third_party_manifest.json",
        docs_root=app_root / "docs",
    )


def stage_portable_release(
    dist_app_root: Path,
    release_base_dir: Path,
    *,
    third_party_source: Path,
    packaging_assets_dir: Path,
    version_label: str,
    source_baseline: str,
) -> tuple[PortableReleaseLayout, ThirdPartyManifest]:
    dist_app_root = Path(dist_app_root)
    if not dist_app_root.exists():
        raise FileNotFoundError(f"PyInstaller dist root does not exist: {dist_app_root}")
    if not (dist_app_root / EXE_NAME).exists():
        raise FileNotFoundError(f"Portable dist is missing {EXE_NAME}: {dist_app_root}")

    layout = build_release_layout(release_base_dir)
    if layout.app_root.exists():
        raise FileExistsError(
            "Release destination already exists; choose a fresh destination directory"
        )

    shutil.copytree(dist_app_root, layout.app_root)
    layout.runtime_root.mkdir(parents=True, exist_ok=True)
    layout.resources_root.mkdir(parents=True, exist_ok=True)
    layout.docs_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(Path(third_party_source), layout.witchybnd_root)

    assets_root = Path(packaging_assets_dir)
    for asset_name in DOC_ASSET_NAMES:
        shutil.copy2(assets_root / asset_name, layout.docs_root / asset_name)

    manifest = build_manifest_for_release(
        layout.app_root,
        version_label=version_label,
        source_baseline=source_baseline,
    )
    manifest.write(layout.manifest_path)
    return layout, manifest
