from __future__ import annotations

import unittest

from ac6_data_manager.emblem import (
    DateTimeBlock,
    EmblemCatalogBuilder,
    EmblemGroup,
    EmblemImage,
    EmblemLayer,
    EmblemPackageService,
    EmblemRecord,
    GroupData,
    UserDataContainer,
    UserDataFile,
)


def make_packaged_record(name: str) -> EmblemRecord:
    return EmblemRecord(
        category=1,
        ugc_id=name,
        creator_id=777,
        datetime_block=DateTimeBlock.now(),
        image=EmblemImage(
            layers=(
                EmblemLayer(
                    group=EmblemGroup(
                        data=GroupData(
                            decal_id=102,
                            pos_x=40,
                            pos_y=-20,
                            scale_x=256,
                            scale_y=512,
                            angle=0,
                            rgba=(0, 255, 0, 255),
                        )
                    )
                ),
            )
        ),
    )


class EmblemPackageTests(unittest.TestCase):
    def test_package_export_import_and_apply(self) -> None:
        record = make_packaged_record("PKG-001")
        source = UserDataContainer.empty().with_files(
            files=(UserDataFile.create("EMBC", record.serialize()),),
        )
        catalog_item = EmblemCatalogBuilder().build(source).items[0]
        service = EmblemPackageService()

        payload = service.export_package([catalog_item])
        imported = service.import_package(payload)
        target = service.apply_package_to_container(UserDataContainer.empty(), imported)

        self.assertEqual(imported.manifest_version, 1)
        self.assertEqual(len(imported.records), 1)
        self.assertEqual(len(target.files), 1)
        target_record = EmblemRecord.deserialize(target.files[0].data)
        self.assertEqual(target_record.ugc_id, "PKG-001")
        self.assertEqual(target_record.category, 2)


if __name__ == "__main__":
    unittest.main()
