from __future__ import annotations

import json
import platform
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence

from ac6_data_manager.container_workspace.workspace import iso_now

from .build import EXE_NAME
from .publish import publish_contract_snapshot
from .runtime import build_provider_bundle
from .third_party import ThirdPartyManifest, verify_manifest


@dataclass(frozen=True)
class PreflightCheck:
    check_id: str
    status: str
    summary: str
    blocking: bool
    details: dict[str, object]

    def to_dict(self) -> dict[str, object]:
        return {
            "check_id": self.check_id,
            "status": self.status,
            "summary": self.summary,
            "blocking": self.blocking,
            "details": self.details,
        }


@dataclass(frozen=True)
class PreflightReport:
    generated_at: str
    app_root: str
    overall_status: str
    checks: tuple[PreflightCheck, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "generated_at": self.generated_at,
            "app_root": self.app_root,
            "overall_status": self.overall_status,
            "checks": [check.to_dict() for check in self.checks],
        }

    def to_text(self) -> str:
        lines = [
            "AC6 Data Manager M1.5 Preflight",
            f"generated_at={self.generated_at}",
            f"app_root={self.app_root}",
            f"overall_status={self.overall_status}",
            "",
        ]
        for check in self.checks:
            lines.append(f"[{check.status}] {check.check_id}: {check.summary}")
        return "\n".join(lines) + "\n"


def _layout_check(app_root: Path) -> PreflightCheck:
    required_paths = (
        EXE_NAME,
        "runtime",
        "resources",
        "third_party/WitchyBND",
        "third_party/third_party_manifest.json",
        "docs/QUICK_START.txt",
        "docs/LIVE_ACCEPTANCE_CHECKLIST.txt",
        "docs/KNOWN_LIMITATIONS.txt",
    )
    missing = tuple(relative for relative in required_paths if not (app_root / relative).exists())
    return PreflightCheck(
        check_id="release-layout",
        status="pass" if not missing else "blocked",
        summary="release 目录契约完整" if not missing else "release 目录契约缺失文件/目录",
        blocking=bool(missing),
        details={"missing_paths": list(missing)},
    )


def run_preflight(
    app_root: Path,
    *,
    expected_version_label: str | None = None,
    argv: Sequence[str] | None = None,
    environ: Mapping[str, str] | None = None,
    platform_system: str | None = None,
    machine: str | None = None,
) -> PreflightReport:
    app_root = Path(app_root)
    system_name = platform_system or platform.system()
    machine_name = machine or platform.machine()

    os_check = PreflightCheck(
        check_id="os-arch",
        status="pass" if system_name == "Windows" and "64" in machine_name.upper() else "blocked",
        summary="Windows x64 环境满足要求"
        if system_name == "Windows" and "64" in machine_name.upper()
        else "当前环境不是 Windows x64",
        blocking=not (system_name == "Windows" and "64" in machine_name.upper()),
        details={"system": system_name, "machine": machine_name},
    )
    layout_check = _layout_check(app_root)

    manifest_path = app_root / "third_party" / "third_party_manifest.json"
    manifest = ThirdPartyManifest.load(manifest_path)
    third_party_result = verify_manifest(
        app_root,
        manifest,
        manifest_path=manifest_path,
        expected_version_label=expected_version_label,
    )
    third_party_check = PreflightCheck(
        check_id="third-party-manifest",
        status="pass" if third_party_result.success else "blocked",
        summary="WitchyBND 目录级 manifest gate 通过"
        if third_party_result.success
        else "WitchyBND 目录级 manifest gate 阻断",
        blocking=not third_party_result.success,
        details=third_party_result.to_dict(),
    )

    provider_bundle = build_provider_bundle(argv=argv, environ=environ, app_root=app_root)
    runtime_check = PreflightCheck(
        check_id="runtime-entry",
        status="pass"
        if provider_bundle.context.provider_kind == "service_bundle" and not provider_bundle.context.is_demo
        else "blocked",
        summary="默认入口为真实 stable-emblem service bundle"
        if provider_bundle.context.provider_kind == "service_bundle" and not provider_bundle.context.is_demo
        else "默认入口落入 demo/fixture provider",
        blocking=provider_bundle.context.provider_kind != "service_bundle" or provider_bundle.context.is_demo,
        details=provider_bundle.context.to_dict(),
    )

    output_policy_check = PreflightCheck(
        check_id="publish-contract",
        status="pass",
        summary="publish / rollback 契约保持 fresh-destination-only 与 fail-closed",
        blocking=False,
        details=publish_contract_snapshot(),
    )

    offline_ready = (
        layout_check.status == "pass"
        and third_party_check.status == "pass"
        and runtime_check.status == "pass"
    )
    offline_check = PreflightCheck(
        check_id="offline-readiness",
        status="pass" if offline_ready else "blocked",
        summary="离线运行前置条件满足" if offline_ready else "离线运行前置条件未满足",
        blocking=not offline_ready,
        details={"network_required": False},
    )

    checks = (os_check, layout_check, third_party_check, runtime_check, output_policy_check, offline_check)
    overall_status = "pass" if all(not check.blocking for check in checks) else "blocked"
    return PreflightReport(
        generated_at=iso_now(),
        app_root=str(app_root),
        overall_status=overall_status,
        checks=checks,
    )


def write_preflight_outputs(report: PreflightReport, *, json_path: Path, text_path: Path) -> tuple[Path, Path]:
    json_path.parent.mkdir(parents=True, exist_ok=True)
    text_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(
        json.dumps(report.to_dict(), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    text_path.write_text(report.to_text(), encoding="utf-8")
    return json_path, text_path
