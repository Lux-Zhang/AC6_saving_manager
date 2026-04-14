from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import UTC, datetime
import hashlib
import io
import json
import zipfile

from .binary import EMBLEM_IMPORTED_USER_CATEGORY, EmblemFormatError, EmblemRecord, UserDataContainer, UserDataFile
from .catalog import EmblemCatalogItem


@dataclass(frozen=True)
class EmblemPackageManifestEntry:
    asset_id: str
    filename: str
    sha256: str
    category: int
    ugc_id: str
    creator_id: int | None
    source_bucket: str
    source_file_index: int


@dataclass(frozen=True)
class EmblemPackage:
    manifest_version: int
    generated_at: str
    records: tuple[tuple[EmblemPackageManifestEntry, EmblemRecord], ...]


class EmblemPackageService:
    package_type = "ac6emblempkg"
    manifest_version = 1

    def export_package(self, items: list[EmblemCatalogItem] | tuple[EmblemCatalogItem, ...]) -> bytes:
        if not items:
            raise EmblemFormatError("Cannot export an empty emblem package")
        buffer = io.BytesIO()
        manifest_entries: list[dict[str, object]] = []
        with zipfile.ZipFile(buffer, mode="w", compression=zipfile.ZIP_DEFLATED) as archive:
            for index, item in enumerate(items, start=1):
                payload = item.record.serialize()
                filename = f"records/{index:04d}_{item.asset_id.replace(':', '_')}.embc"
                checksum = hashlib.sha256(payload).hexdigest()
                archive.writestr(filename, payload)
                manifest_entries.append(
                    asdict(
                        EmblemPackageManifestEntry(
                            asset_id=item.asset_id,
                            filename=filename,
                            sha256=checksum,
                            category=item.record.category,
                            ugc_id=item.record.ugc_id,
                            creator_id=item.record.creator_id,
                            source_bucket=item.source_bucket,
                            source_file_index=item.file_index,
                        )
                    )
                )
            manifest = {
                "package_type": self.package_type,
                "version": self.manifest_version,
                "generated_at": datetime.now(tz=UTC).isoformat(),
                "records": manifest_entries,
            }
            archive.writestr(
                "manifest.json",
                json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True),
            )
        return buffer.getvalue()

    def import_package(self, payload: bytes) -> EmblemPackage:
        buffer = io.BytesIO(payload)
        with zipfile.ZipFile(buffer, mode="r") as archive:
            try:
                manifest = json.loads(archive.read("manifest.json").decode("utf-8"))
            except KeyError as exc:
                raise EmblemFormatError("ac6emblempkg v1 is missing manifest.json") from exc
            if manifest.get("package_type") != self.package_type:
                raise EmblemFormatError("Unsupported package_type for emblem package import")
            if manifest.get("version") != self.manifest_version:
                raise EmblemFormatError("Unsupported ac6emblempkg version")
            records: list[tuple[EmblemPackageManifestEntry, EmblemRecord]] = []
            for raw_entry in manifest.get("records", []):
                entry = EmblemPackageManifestEntry(
                    asset_id=raw_entry["asset_id"],
                    filename=raw_entry["filename"],
                    sha256=raw_entry["sha256"],
                    category=raw_entry["category"],
                    ugc_id=raw_entry["ugc_id"],
                    creator_id=raw_entry.get("creator_id"),
                    source_bucket=raw_entry["source_bucket"],
                    source_file_index=raw_entry["source_file_index"],
                )
                file_payload = archive.read(entry.filename)
                checksum = hashlib.sha256(file_payload).hexdigest()
                if checksum != entry.sha256:
                    raise EmblemFormatError(
                        f"Checksum mismatch for packaged emblem {entry.asset_id}; fail-closed"
                    )
                records.append((entry, EmblemRecord.deserialize(file_payload)))
            return EmblemPackage(
                manifest_version=self.manifest_version,
                generated_at=manifest["generated_at"],
                records=tuple(records),
            )

    def apply_package_to_container(
        self,
        container: UserDataContainer,
        package: EmblemPackage,
        *,
        target_category: int = EMBLEM_IMPORTED_USER_CATEGORY,
    ) -> UserDataContainer:
        next_files = list(container.files)
        for _, record in package.records:
            user_record = record.as_user_editable(target_category=target_category)
            next_files.append(UserDataFile.create("EMBC", user_record.serialize()))
        return container.with_files(files=tuple(next_files))
