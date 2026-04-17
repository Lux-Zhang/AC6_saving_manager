#include "exchange_contracts.hpp"

#include <utility>

namespace ac6dm::exchange {
namespace {

using ac6dm::contracts::DetailField;
using ac6dm::contracts::ExchangeAssetKind;
using ac6dm::contracts::ExchangeFormatContract;
using ac6dm::contracts::FutureActionAvailability;
using ac6dm::contracts::FutureActionContract;
using ac6dm::contracts::FutureActionKind;

std::vector<DetailField> commonRequiredFields() {
    return {
        DetailField{"format_version", "固定为 v1；用于后续兼容判断。"},
        DetailField{"asset_kind", "emblem 或 ac；防止 GUI 与导入器误用。"},
        DetailField{"source_metadata", "来源摘要，仅用于显示与审计，不直接驱动写入。"},
        DetailField{"preview_metadata", "记录预览 provenance 与摘要，不承诺本阶段真实位图导出。"},
        DetailField{"record_payload", "资产记录负载；N2A 仅冻结字段，不实现序列化。"},
        DetailField{"compatibility_info", "兼容信息；至少保留 producer 与最小 reader 版本位。"},
    };
}

std::vector<DetailField> commonCompatibilityFields() {
    return {
        DetailField{"producer", "写出该文件的程序标识与版本。"},
        DetailField{"min_reader_version", "最小读取版本；未来导入前置检查使用。"},
        DetailField{"payload_schema", "负载结构名；用于后续增量演进。"},
    };
}

FutureActionContract makeDisabledAction(
    FutureActionKind actionKind,
    std::string actionId,
    std::string label,
    std::string disabledReasonCode,
    std::string disabledReasonText,
    std::string unlockStage,
    std::string policyNote) {
    FutureActionContract contract;
    contract.actionKind = actionKind;
    contract.actionId = std::move(actionId);
    contract.label = std::move(label);
    contract.availability = FutureActionAvailability::Disabled;
    contract.disabledReasonCode = std::move(disabledReasonCode);
    contract.disabledReasonText = std::move(disabledReasonText);
    contract.unlockStage = std::move(unlockStage);
    contract.implementationStatus = "contract_frozen_only";
    contract.policyNote = std::move(policyNote);
    return contract;
}

}  // namespace

ExchangeFormatContract describeMinimalExchangeFormat(const ExchangeAssetKind assetKind) {
    ExchangeFormatContract contract;
    contract.formatVersion = std::string(ac6dm::contracts::kExchangeFormatVersionV1);
    contract.assetKind = assetKind;
    contract.requiredFields = commonRequiredFields();
    contract.compatibilityFields = commonCompatibilityFields();
    contract.payloadPolicy =
        "N2A 仅冻结最小合同字段；禁止真实文件导入导出、禁止真实序列化与反序列化。";
    contract.implementationStatus = "contract_frozen_only";

    if (assetKind == ExchangeAssetKind::Emblem) {
        contract.fileExtension = std::string(ac6dm::contracts::kAc6EmblemDataExtension);
        contract.containerLabel = "AC6 Emblem Exchange Data";
    } else {
        contract.fileExtension = std::string(ac6dm::contracts::kAc6AcDataExtension);
        contract.containerLabel = "AC6 AC Exchange Data";
    }

    return contract;
}

std::vector<FutureActionContract> buildN2AFutureActionContracts() {
    return {
        makeDisabledAction(
            FutureActionKind::ImportEmblemFileToUserSlot,
            "future.import-emblem-file-to-user-slot",
            "导入徽章文件到使用者栏",
            std::string(ac6dm::contracts::kReasonCodeEmblemExchangeDeferred),
            "N2A 只冻结 .ac6emblemdata 接口；真实徽章文件导入需待 N2B 完成后开放。",
            "N2B",
            "本阶段只允许存档内分享徽章 -> 使用者栏的稳定导入。"
        ),
        makeDisabledAction(
            FutureActionKind::ExportSelectedEmblem,
            "future.export-selected-emblem",
            "导出所选徽章",
            std::string(ac6dm::contracts::kReasonCodeEmblemExchangeDeferred),
            "N2A 只冻结 .ac6emblemdata 接口；真实徽章文件导出需待 N2B 完成后开放。",
            "N2B",
            "GUI 可先绑定 disabled reason，不得写出任何真实文件。"
        ),
        makeDisabledAction(
            FutureActionKind::CopySelectedAcToUserSlot,
            "future.copy-selected-ac-to-user-slot",
            "将所选AC导入使用者栏",
            std::string(ac6dm::contracts::kReasonCodeAcGateLocked),
            "AC 仍处于只读资格认证阶段；需待 Gate-AC-01/02/03 全部通过后开放。",
            "N2C/N2D",
            "N2A 允许浏览 AC 五类来源、详情与 preview provenance，但禁止 AC mutation。"
        ),
        makeDisabledAction(
            FutureActionKind::ImportAcFileToUserSlot,
            "future.import-ac-file-to-user-slot",
            "导入AC文件到使用者栏",
            std::string(ac6dm::contracts::kReasonCodeAcGateLocked),
            "AC 文件交换需要 record-map、preview provenance 与指定栏位写入资格全部稳定后开放。",
            "N2D",
            "本阶段仅冻结 .ac6acdata 最小合同类型，不实现文件读取。"
        ),
        makeDisabledAction(
            FutureActionKind::ExportSelectedAc,
            "future.export-selected-ac",
            "导出所选AC",
            std::string(ac6dm::contracts::kReasonCodeAcGateLocked),
            "AC 文件导出需待 N2C 资格认证完成并进入 N2D 后开放。",
            "N2D",
            "GUI 可先展示 disabled reason，但不得写出任何真实 AC 文件。"
        ),
    };
}

}  // namespace ac6dm::exchange
