#pragma once

#include "config/sidecar/ThirdPartyManifest.hpp"
#include "platform/windows/process/ProcessRunner.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ac6dm::tool_adapter {

struct SidecarRunResult final {
    platform::ProcessReport process;
    std::filesystem::path outputPath;
};

class WitchyBndProcessAdapter final {
public:
    explicit WitchyBndProcessAdapter(
        std::filesystem::path appRoot,
        int timeoutMs = 300000);

    sidecar::ManifestVerificationResult verifyBundledManifest(
        const std::optional<std::string>& expectedVersionLabel = std::nullopt) const;

    std::filesystem::path resolveEntrypoint() const;
    platform::ProcessReport probeVersion() const;
    SidecarRunResult unpack(
        const std::filesystem::path& containerPath,
        const std::optional<std::filesystem::path>& expectedDirectory = std::nullopt) const;
    SidecarRunResult repack(
        const std::filesystem::path& unpackedDirectory,
        const std::optional<std::filesystem::path>& expectedContainer = std::nullopt) const;

private:
    std::filesystem::path appRoot_;
    int timeoutMs_{300000};
    platform::ProcessRunner processRunner_{};
};

}  // namespace ac6dm::tool_adapter
