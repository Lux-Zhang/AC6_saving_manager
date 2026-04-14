from __future__ import annotations

import sys

from PyQt5.QtWidgets import QApplication

from .gui import (
    Ac6DataManagerWindow,
    AuditEntryDto,
    CatalogItemDto,
    DeleteImpactPlanDto,
    DetailFieldDto,
    FixtureGuiDataProvider,
    ImportOperationDto,
    ImportPlanDto,
    PreviewHandleDto,
)
from .gui.main_window import create_application




def build_demo_provider() -> FixtureGuiDataProvider:
    item = CatalogItemDto(
        asset_id='emblem-user-001',
        display_name='Steel Haze Mark',
        slot_id='USER_EMBLEM_07',
        origin='user',
        editable_state='editable',
        source_label='share/manual-pick',
        preview=PreviewHandleDto(
            label='preview',
            provenance='direct extract',
            source_hint='fixture/demo',
            image_bytes=None,
            note='演示预览图来自 fixture。',
        ),
        description='演示数据：展示 M1 徽章库详情、预览与风险状态。',
        tags=('favorite', 'pvp', 'manual-import'),
        dependency_references=('restore-point-required', 'audit-visible'),
        detail_fields=(
            DetailFieldDto('作者', 'fixture'),
            DetailFieldDto('导入通道', 'share -> user'),
        ),
    )
    import_plan = ImportPlanDto(
        title='ImportPlan / Steel Haze Mark',
        source_asset_ids=(item.asset_id,),
        target_slot='USER_EMBLEM_07',
        mode='dry_run_then_apply',
        summary='先在 shadow workspace 中执行 dry-run，再生成 restore point、写回并做 post-write readback。',
        operations=(
            ImportOperationDto('选择分享徽章', 'share catalog', 'selected'),
            ImportOperationDto('槽位分配', 'USER_EMBLEM_07', 'compatible'),
            ImportOperationDto('写后回读', 'readback', 'required', 'M1 stable lane hard gate'),
        ),
        warnings=('禁止 in-place overwrite', '失败时必须 fail-closed 并保留 incident artifact'),
    )
    delete_plan = DeleteImpactPlanDto(
        title='DeleteImpactPlan / Steel Haze Mark',
        asset_id=item.asset_id,
        summary='当前仅展示删除影响分析原型，不做真实删除。',
        impacted_assets=('贴花布局 A', '分享映射缓存'),
        warnings=('删除功能未开放', '需先人工确认依赖图'),
        risk_level='read_only',
        allowed=False,
    )
    audit = AuditEntryDto(
        timestamp='2026-04-14T06:30:00Z',
        action='emblem_import_dry_run',
        target=item.asset_id,
        result_status='dry_run_ready',
        summary='fixture 导入计划已生成，等待 Apply。',
        restore_point_id='rp-demo-001',
        details=('toolchain=WitchyBND', 'shadow workspace prepared', 'readback pending'),
    )
    return FixtureGuiDataProvider(
        catalog_items=(item,),
        import_plans_by_assets={(item.asset_id,): import_plan},
        delete_plans_by_asset={item.asset_id: delete_plan},
        audit_entries=(audit,),
    )


def main(argv: list[str] | None = None) -> int:
    _ = argv or sys.argv
    app = create_application()
    window = Ac6DataManagerWindow(build_demo_provider())
    window.show()
    return app.exec_()


if __name__ == '__main__':
    raise SystemExit(main())
