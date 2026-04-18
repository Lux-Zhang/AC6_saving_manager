#pragma once

#include "ac_regulation_catalog.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ac6dm::ac {

struct RuntimeRegulationSnapshotInfo final {
    std::filesystem::path sourceFile;
    bool filePresent{false};
    bool decrypted{false};
    bool headerQualified{false};
    std::uint32_t containerSizeField{0};
    std::string containerMagic;
    std::uint32_t versionField{0};
    std::uint32_t buildField{0};
    std::uint32_t embeddedOffset{20};
    std::uint64_t plaintextBytes{0};
    std::string plaintextSha256;
    bool matchesValidatedSnapshot{false};
    std::string note;
};

RuntimeRegulationSnapshotInfo inspectRegulationSnapshotPlaintext(
    const std::vector<std::uint8_t>& plaintext,
    std::filesystem::path sourceFile = {});

RuntimeRegulationSnapshotInfo inspectRuntimeRegulationSnapshot(
    const std::filesystem::path& unpackedSaveDir);

const RegulationPresetCatalog* resolveRuntimeRegulationCatalog(
    const std::filesystem::path& unpackedSaveDir,
    RuntimeRegulationSnapshotInfo* outInfo = nullptr);

}  // namespace ac6dm::ac
