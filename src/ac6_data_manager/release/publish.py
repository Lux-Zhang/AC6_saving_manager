from __future__ import annotations

import json
import shutil
from pathlib import Path

from ac6_data_manager.container_workspace import PackTransactionResult
from ac6_data_manager.validation import RollbackResult


def _slugify_timestamp(timestamp: str) -> str:
    return (
        timestamp.replace(":", "")
        .replace("-", "")
        .replace("T", "-")
        .replace("Z", "")
    )


def publish_contract_snapshot() -> dict[str, object]:
    return {
        "dry_run_before_apply": True,
        "shadow_workspace": True,
        "post_write_readback": True,
        "no_in_place_overwrite": True,
        "fresh_destination_only": True,
        "fail_closed": True,
        "rollback_fresh_destination_only": True,
    }


def write_publish_artifacts(
    result: PackTransactionResult,
    artifacts_root: Path,
    *,
    incident_root: Path | None = None,
) -> dict[str, str]:
    artifacts_root = Path(artifacts_root)
    artifacts_root.mkdir(parents=True, exist_ok=True)
    stamp = _slugify_timestamp(result.audit_entry.timestamp)
    publish_path = artifacts_root / f"publish-audit-{stamp}.json"
    payload = result.to_dict()
    payload["contract"] = publish_contract_snapshot()
    publish_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    output = {"publish_audit": str(publish_path)}

    if result.readback_report is not None:
        readback_path = artifacts_root / f"readback-{stamp}.json"
        readback_path.write_text(
            json.dumps(
                {
                    "workspace_id": result.workspace_id,
                    "audit_timestamp": result.audit_entry.timestamp,
                    "readback_report": result.to_dict()["readback_report"],
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        output["readback"] = str(readback_path)

    if result.incident_bundle is not None:
        destination_root = Path(incident_root or artifacts_root / "incident")
        destination_root.mkdir(parents=True, exist_ok=True)
        incident_copy = destination_root / f"incident-{stamp}.json"
        shutil.copy2(result.incident_bundle, incident_copy)
        output["incident_artifact"] = str(incident_copy)

    return output


def write_rollback_artifact(result: RollbackResult, artifacts_root: Path) -> str:
    artifacts_root = Path(artifacts_root)
    artifacts_root.mkdir(parents=True, exist_ok=True)
    stamp = _slugify_timestamp(result.audit_event.timestamp) if result.audit_event else result.restore_point_id
    rollback_path = artifacts_root / f"rollback-{stamp}.json"
    rollback_path.write_text(
        json.dumps(
            {
                "restore_point_id": result.restore_point_id,
                "workspace_id": result.workspace_id,
                "backup_path": str(result.backup_path),
                "output_path": str(result.output_path),
                "restored_sha256": result.restored_sha256,
                "source_sha256": result.source_sha256,
                "backup_sha256": result.backup_sha256,
                "audit_event": None if result.audit_event is None else result.audit_event.to_dict(),
                "contract": publish_contract_snapshot(),
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return str(rollback_path)
