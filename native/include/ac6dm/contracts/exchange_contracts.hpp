#pragma once

#include "ac6dm/contracts/common_types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ac6dm::contracts {

enum class ExchangeAssetKind {
    Emblem,
    Ac,
};

enum class FutureActionAvailability {
    Enabled,
    Disabled,
};

enum class FutureActionKind {
    ImportEmblemFileToUserSlot,
    ExportSelectedEmblem,
    CopySelectedAcToUserSlot,
    ImportAcFileToUserSlot,
    ExportSelectedAc,
};

struct ExchangeFormatContract final {
    std::string formatVersion;
    ExchangeAssetKind assetKind{ExchangeAssetKind::Emblem};
    std::string fileExtension;
    std::string containerLabel;
    std::vector<DetailField> requiredFields;
    std::vector<DetailField> compatibilityFields;
    std::string payloadPolicy;
    std::string implementationStatus;
};

struct FutureActionContract final {
    FutureActionKind actionKind{FutureActionKind::ImportEmblemFileToUserSlot};
    std::string actionId;
    std::string label;
    FutureActionAvailability availability{FutureActionAvailability::Disabled};
    std::string disabledReasonCode;
    std::string disabledReasonText;
    std::string unlockStage;
    std::string implementationStatus;
    std::string policyNote;
};

inline constexpr std::string_view kAc6EmblemDataExtension = ".ac6emblemdata";
inline constexpr std::string_view kAc6AcDataExtension = ".ac6acdata";
inline constexpr std::string_view kExchangeFormatVersionV1 = "1";

inline constexpr std::string_view kReasonCodeEmblemExchangeDeferred =
    "future.exchange.emblem.deferred.until-n2b";
inline constexpr std::string_view kReasonCodeAcGateLocked =
    "future.exchange.ac.locked.pending-gate";

}  // namespace ac6dm::contracts
