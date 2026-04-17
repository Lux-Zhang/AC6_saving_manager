#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::sidecar {

struct ThirdPartyManifestFile final {
    std::string path;
    std::string sha256;
    bool required{true};
    std::string fileRole;
    std::uintmax_t sizeBytes{0};
};

struct ThirdPartyManifest final {
    int schemaVersion{1};
    std::string name;
    std::string sourceBaseline;
    std::string versionLabel;
    std::string entrypoint;
    bool required{true};
    std::string preflightPolicy;
    std::string generatedAt;
    std::vector<ThirdPartyManifestFile> files;

    static ThirdPartyManifest load(const std::filesystem::path& manifestPath);
};

struct ManifestVerificationResult final {
    bool success{false};
    std::filesystem::path manifestPath;
    std::filesystem::path entrypointPath;
    std::string actualVersionLabel;
    std::optional<std::string> expectedVersionLabel;
    std::vector<std::string> missingFiles;
    std::vector<std::string> extraFiles;
    std::vector<std::string> hashMismatches;
    std::vector<std::string> errors;
};

std::string sha256File(const std::filesystem::path& filePath);
ManifestVerificationResult verifyManifest(
    const std::filesystem::path& appRoot,
    const ThirdPartyManifest& manifest,
    const std::filesystem::path& manifestPath,
    const std::optional<std::string>& expectedVersionLabel = std::nullopt);

}  // namespace ac6dm::sidecar
