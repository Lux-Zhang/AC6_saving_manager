from __future__ import annotations

import unittest

from ac6_data_manager.emblem import (
    DateTimeBlock,
    EmblemGroup,
    EmblemImage,
    EmblemLayer,
    EmblemPreviewRenderer,
    EmblemRecord,
    GroupData,
)


class EmblemPreviewTests(unittest.TestCase):
    def test_preview_renders_basic_shapes_and_fail_soft_placeholder_for_unknown_decal(self) -> None:
        record = EmblemRecord(
            category=1,
            ugc_id="PREVIEW",
            creator_id=None,
            datetime_block=DateTimeBlock.now(),
            image=EmblemImage(
                layers=(
                    EmblemLayer(
                        group=EmblemGroup(
                            data=GroupData(
                                decal_id=100,
                                pos_x=0,
                                pos_y=0,
                                scale_x=640,
                                scale_y=640,
                                angle=0,
                                rgba=(255, 0, 0, 255),
                            )
                        )
                    ),
                    EmblemLayer(
                        group=EmblemGroup(
                            data=GroupData(
                                decal_id=999,
                                pos_x=400,
                                pos_y=0,
                                scale_x=320,
                                scale_y=320,
                                angle=0,
                                rgba=(0, 0, 255, 255),
                            )
                        )
                    ),
                )
            ),
        )

        result = EmblemPreviewRenderer(canvas_size=256).render(record)

        self.assertEqual(result.placeholder_count, 1)
        self.assertEqual(result.provenance, ("pil-basic-shapes-v1",))
        self.assertEqual(len(result.warnings), 1)
        self.assertIn("decal[999]", result.warnings[0].provenance)
        self.assertIsNotNone(result.image.getbbox())


if __name__ == "__main__":
    unittest.main()
