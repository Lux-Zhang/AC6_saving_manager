#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::contracts {

enum class AssetOrigin {
    User,
    Share,
    Package,
};

enum class PreviewKind {
    None,
    Png,
    Binary,
};

enum class ResultCode {
    Ok,
    InvalidInput,
    UnsupportedEnvironment,
    ManifestBlocked,
    SidecarInvocationFailed,
    SaveOpenFailed,
    ImportPlanBlocked,
    NoEmptyUserSlot,
    ReadbackFailed,
    OutputCollision,
    InternalError,
};

struct DetailField final {
    std::string label;
    std::string value;
};

struct PreviewHandle final {
    PreviewKind kind{PreviewKind::None};
    std::string label;
    std::string provenance;
    std::optional<std::string> sourceHint;
    std::optional<std::string> note;
    std::optional<std::string> mediaType;
    std::optional<std::string> sha256;
    std::vector<unsigned char> imageBytes;
};

struct DiagnosticEntry final {
    std::string code;
    std::string message;
    std::vector<std::string> details;
};

struct EvidencePath final {
    std::filesystem::path path;
    std::string role;
};

}  // namespace ac6dm::contracts
