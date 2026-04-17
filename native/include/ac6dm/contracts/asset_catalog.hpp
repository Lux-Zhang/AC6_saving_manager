#pragma once

#include "ac6dm/contracts/common_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ac6dm::contracts {

enum class AssetKind {
    Emblem,
    Ac,
};

enum class SourceBucket {
    User1,
    User2,
    User3,
    User4,
    Share,
};

enum class PreviewState {
    NativeEmbedded,
    DerivedRender,
    Unavailable,
    Unknown,
};

enum class WriteCapability {
    Editable,
    ReadOnly,
    LockedPendingGate,
};

inline std::string toString(const AssetKind value) {
    switch (value) {
    case AssetKind::Emblem:
        return "emblem";
    case AssetKind::Ac:
        return "ac";
    }
    return "emblem";
}

inline std::string toString(const SourceBucket value) {
    switch (value) {
    case SourceBucket::User1:
        return "user1";
    case SourceBucket::User2:
        return "user2";
    case SourceBucket::User3:
        return "user3";
    case SourceBucket::User4:
        return "user4";
    case SourceBucket::Share:
        return "share";
    }
    return "share";
}

inline std::string toDisplayLabel(const SourceBucket value) {
    switch (value) {
    case SourceBucket::User1:
        return "使用者1";
    case SourceBucket::User2:
        return "使用者2";
    case SourceBucket::User3:
        return "使用者3";
    case SourceBucket::User4:
        return "使用者4";
    case SourceBucket::Share:
        return "分享";
    }
    return "分享";
}

inline std::optional<int> slotIndexOf(const SourceBucket value) {
    switch (value) {
    case SourceBucket::User1:
        return 0;
    case SourceBucket::User2:
        return 1;
    case SourceBucket::User3:
        return 2;
    case SourceBucket::User4:
        return 3;
    case SourceBucket::Share:
        return std::nullopt;
    }
    return std::nullopt;
}

inline std::string toString(const PreviewState value) {
    switch (value) {
    case PreviewState::NativeEmbedded:
        return "native_embedded";
    case PreviewState::DerivedRender:
        return "derived_render";
    case PreviewState::Unavailable:
        return "unavailable";
    case PreviewState::Unknown:
        return "unknown";
    }
    return "unknown";
}

inline std::string toString(const WriteCapability value) {
    switch (value) {
    case WriteCapability::Editable:
        return "editable";
    case WriteCapability::ReadOnly:
        return "read_only";
    case WriteCapability::LockedPendingGate:
        return "locked_pending_gate";
    }
    return "read_only";
}

struct CatalogItemDto final {
    std::string assetId;
    AssetKind assetKind{AssetKind::Emblem};
    SourceBucket sourceBucket{SourceBucket::Share};
    std::optional<int> slotIndex;
    std::string displayName;
    std::string archiveName;
    std::string machineName;
    std::string shareCode;
    PreviewState previewState{PreviewState::Unknown};
    WriteCapability writeCapability{WriteCapability::ReadOnly};
    std::string recordRef;
    std::string detailProvenance;
    std::string slotLabel;
    AssetOrigin origin{AssetOrigin::Share};
    bool editable{false};
    std::string sourceLabel;
    PreviewHandle preview;
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> dependencyRefs;
    std::vector<DetailField> detailFields;
};

}  // namespace ac6dm::contracts
