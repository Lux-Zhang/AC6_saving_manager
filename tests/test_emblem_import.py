from __future__ import annotations

import unittest

from ac6_data_manager.emblem import (
    DateTimeBlock,
    EmblemCatalogBuilder,
    EmblemGroup,
    EmblemImage,
    EmblemLayer,
    EmblemRecord,
    GroupData,
    ShareEmblemImporter,
    UserDataContainer,
    UserDataFile,
)


def make_record(*, category: int, ugc_id: str) -> EmblemRecord:
    return EmblemRecord(
        category=category,
        ugc_id=ugc_id,
        creator_id=42,
        datetime_block=DateTimeBlock.now(),
        image=EmblemImage(
            layers=(
                EmblemLayer(
                    group=EmblemGroup(
                        data=GroupData(
                            decal_id=100,
                            pos_x=0,
                            pos_y=0,
                            scale_x=512,
                            scale_y=512,
                            angle=0,
                            rgba=(255, 255, 255, 255),
                        )
                    )
                ),
            )
        ),
    )


class ShareImportTests(unittest.TestCase):
    def test_selective_share_import_only_copies_selected_emblems(self) -> None:
        share_a = make_record(category=9, ugc_id="SHARE-A")
        share_b = make_record(category=9, ugc_id="SHARE-B")
        container = UserDataContainer.empty().with_files(
            files=(UserDataFile.create("EMBC", make_record(category=1, ugc_id="USER-0").serialize()),),
            extra_files=(
                UserDataFile.create("EMBC", share_a.serialize()),
                UserDataFile.create("TEXT", b"skip me"),
                UserDataFile.create("EMBC", share_b.serialize()),
            ),
        )

        result = ShareEmblemImporter().import_selected(container, selected_extra_indexes=[2])

        self.assertEqual(len(result.container.files), 2)
        imported_record = EmblemRecord.deserialize(result.container.files[1].data)
        self.assertEqual(imported_record.ugc_id, "SHARE-B")
        self.assertEqual(imported_record.category, 2)
        self.assertEqual(result.imported[0].source_asset_id, "extraFiles:2")
        self.assertEqual(result.imported[0].destination_asset_id, "files:1")

    def test_catalog_marks_user_files_editable_and_share_files_non_editable(self) -> None:
        container = UserDataContainer.empty().with_files(
            files=(UserDataFile.create("EMBC", make_record(category=2, ugc_id="USER").serialize()),),
            extra_files=(UserDataFile.create("EMBC", make_record(category=9, ugc_id="SHARE").serialize()),),
        )

        catalog = EmblemCatalogBuilder().build(container)
        by_id = catalog.by_asset_id()

        self.assertTrue(by_id["files:0"].editable)
        self.assertFalse(by_id["extraFiles:0"].editable)


if __name__ == "__main__":
    unittest.main()
