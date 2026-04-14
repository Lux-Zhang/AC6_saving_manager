from __future__ import annotations

import unittest

from ac6_data_manager.emblem import (
    DateTimeBlock,
    EmblemGroup,
    EmblemImage,
    EmblemLayer,
    EmblemRecord,
    GroupData,
    UserDataContainer,
    UserDataFile,
)


class EmblemBinaryTests(unittest.TestCase):
    def _record(self, *, category: int = 1, ugc_id: str = "", decal_id: int = 100) -> EmblemRecord:
        image = EmblemImage(
            layers=(
                EmblemLayer(
                    group=EmblemGroup(
                        data=GroupData(
                            decal_id=decal_id,
                            pos_x=0,
                            pos_y=0,
                            scale_x=640,
                            scale_y=640,
                            angle=0,
                            rgba=(255, 0, 0, 255),
                        )
                    )
                ),
            )
        )
        return EmblemRecord(
            category=category,
            ugc_id=ugc_id,
            creator_id=123456,
            datetime_block=DateTimeBlock.now(),
            image=image,
        )

    def test_user_data007_round_trip_preserves_files_and_extra_files(self) -> None:
        user_record = self._record(category=1)
        share_record = self._record(category=9, ugc_id="SHARE-001")
        container = UserDataContainer.empty(iv=b"0123456789ABCDEF").with_files(
            files=(UserDataFile.create("EMBC", user_record.serialize()),),
            extra_files=(
                UserDataFile.create("EMBC", share_record.serialize()),
                UserDataFile.create("TEXT", b"ignored"),
            ),
        )

        encrypted = container.serialize_encrypted()
        parsed = UserDataContainer.deserialize_encrypted(encrypted)

        self.assertEqual(len(parsed.files), 1)
        self.assertEqual(len(parsed.extra_files), 2)
        reparsed_user = EmblemRecord.deserialize(parsed.files[0].data)
        reparsed_share = EmblemRecord.deserialize(parsed.extra_files[0].data)
        self.assertEqual(reparsed_user.category, 1)
        self.assertEqual(reparsed_share.ugc_id, "SHARE-001")
        self.assertEqual(parsed.extra_files[1].file_type, "TEXT")


if __name__ == "__main__":
    unittest.main()
