from __future__ import annotations

from dataclasses import dataclass

from .binary import (
    EMBLEM_IMPORTED_USER_CATEGORY,
    EmblemFormatError,
    EmblemRecord,
    UserDataContainer,
    UserDataFile,
)
from .catalog import EmblemCatalog, EmblemCatalogBuilder


@dataclass(frozen=True)
class SelectiveImportEntry:
    source_extra_index: int
    destination_file_index: int
    source_asset_id: str
    destination_asset_id: str
    target_category: int


@dataclass(frozen=True)
class SelectiveImportResult:
    container: UserDataContainer
    imported: tuple[SelectiveImportEntry, ...]
    catalog: EmblemCatalog


class ShareEmblemImporter:
    def __init__(self, *, target_category: int = EMBLEM_IMPORTED_USER_CATEGORY) -> None:
        self.target_category = target_category
        self.catalog_builder = EmblemCatalogBuilder()

    def import_selected(
        self,
        container: UserDataContainer,
        *,
        selected_extra_indexes: list[int] | tuple[int, ...],
    ) -> SelectiveImportResult:
        if not selected_extra_indexes:
            raise EmblemFormatError("No share emblems were selected for import")
        next_files = list(container.files)
        imported: list[SelectiveImportEntry] = []
        seen_indexes: set[int] = set()
        for extra_index in selected_extra_indexes:
            if extra_index in seen_indexes:
                raise EmblemFormatError(f"Duplicate share emblem selection: extraFiles[{extra_index}]")
            seen_indexes.add(extra_index)
            try:
                share_file = container.extra_files[extra_index]
            except IndexError as exc:
                raise EmblemFormatError(
                    f"Selected share emblem index extraFiles[{extra_index}] does not exist"
                ) from exc
            if share_file.file_type != "EMBC":
                raise EmblemFormatError(
                    f"Selected extraFiles[{extra_index}] is {share_file.file_type}, expected EMBC"
                )
            source_record = EmblemRecord.deserialize(share_file.data)
            imported_record = source_record.as_user_editable(target_category=self.target_category)
            imported_file = UserDataFile.create("EMBC", imported_record.serialize())
            destination_index = len(next_files)
            next_files.append(imported_file)
            imported.append(
                SelectiveImportEntry(
                    source_extra_index=extra_index,
                    destination_file_index=destination_index,
                    source_asset_id=f"extraFiles:{extra_index}",
                    destination_asset_id=f"files:{destination_index}",
                    target_category=self.target_category,
                )
            )
        next_container = container.with_files(files=tuple(next_files))
        return SelectiveImportResult(
            container=next_container,
            imported=tuple(imported),
            catalog=self.catalog_builder.build(next_container),
        )
