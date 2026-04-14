from __future__ import annotations

import json
from io import BytesIO
from zipfile import ZIP_DEFLATED, ZipFile

from .models import EmblemAsset, EmblemOrigin, PackageV1

PACKAGE_TYPE = "ac6emblempkg"
SCHEMA_VERSION = 1
MANIFEST_PATH = "manifest.json"


def _asset_slug(asset: EmblemAsset, index: int) -> str:
    safe_id = "".join(ch if ch.isalnum() or ch in ("-", "_") else "-" for ch in asset.asset_id)
    return f"{index:03d}-{safe_id}"


def export_ac6emblempkg_v1(
    assets: list[EmblemAsset] | tuple[EmblemAsset, ...],
) -> bytes:
    if not assets:
        raise ValueError("ac6emblempkg v1 export requires at least one emblem asset")

    manifest_entries: list[dict[str, object]] = []
    buffer = BytesIO()

    with ZipFile(buffer, mode="w", compression=ZIP_DEFLATED) as archive:
        for index, asset in enumerate(assets, start=1):
            slug = _asset_slug(asset, index)
            payload_path = f"emblems/{slug}.embc"
            archive.writestr(payload_path, asset.payload)

            preview_path: str | None = None
            if asset.preview_bytes is not None:
                preview_path = f"previews/{slug}.bin"
                archive.writestr(preview_path, asset.preview_bytes)

            manifest_entries.append(
                {
                    "asset_id": asset.asset_id,
                    "title": asset.title,
                    "origin": asset.origin.value,
                    "category": asset.category,
                    "slot_id": asset.slot_id,
                    "ugc_id": asset.ugc_id,
                    "creator_id": asset.creator_id,
                    "created_at": asset.created_at,
                    "payload_path": payload_path,
                    "payload_sha256": asset.payload_sha256,
                    "payload_size": len(asset.payload),
                    "preview_path": preview_path,
                    "preview_sha256": asset.preview_sha256,
                    "preview_size": len(asset.preview_bytes or b""),
                    "dependency_refs": list(asset.dependency_refs),
                }
            )

        manifest = {
            "package_type": PACKAGE_TYPE,
            "schema_version": SCHEMA_VERSION,
            "compatibility": {
                "stable_lane": "emblem-only",
                "ac_mutation": "unsupported",
            },
            "emblems": manifest_entries,
        }
        archive.writestr(
            MANIFEST_PATH,
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True),
        )

    return buffer.getvalue()


def import_ac6emblempkg_v1(package_bytes: bytes) -> PackageV1:
    with ZipFile(BytesIO(package_bytes), mode="r") as archive:
        manifest = json.loads(archive.read(MANIFEST_PATH).decode("utf-8"))
        if manifest.get("package_type") != PACKAGE_TYPE:
            raise ValueError("unsupported emblem package type")
        if manifest.get("schema_version") != SCHEMA_VERSION:
            raise ValueError("unsupported emblem package schema version")

        assets: list[EmblemAsset] = []
        for entry in manifest["emblems"]:
            payload = archive.read(entry["payload_path"])
            preview_path = entry.get("preview_path")
            preview = archive.read(preview_path) if preview_path else None

            asset = EmblemAsset(
                asset_id=entry["asset_id"],
                title=entry["title"],
                payload=payload,
                preview_bytes=preview,
                origin=EmblemOrigin.PACKAGE,
                storage_bucket="package",
                category=entry["category"],
                slot_id=entry.get("slot_id"),
                ugc_id=entry.get("ugc_id") or "",
                creator_id=entry.get("creator_id"),
                created_at=entry.get("created_at"),
                dependency_refs=tuple(entry.get("dependency_refs", [])),
            )

            if asset.payload_sha256 != entry["payload_sha256"]:
                raise ValueError(
                    f"payload checksum mismatch for emblem asset: {asset.asset_id}"
                )
            if preview is not None and asset.preview_sha256 != entry["preview_sha256"]:
                raise ValueError(
                    f"preview checksum mismatch for emblem asset: {asset.asset_id}"
                )

            assets.append(asset)

    return PackageV1(
        manifest=manifest,
        assets=tuple(assets),
        raw_bytes=package_bytes,
    )

