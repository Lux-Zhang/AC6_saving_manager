#pragma once

#include "ac6dm/contracts/asset_catalog.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace ac6dm::ac {

std::optional<contracts::AcPreviewDto> tryBuildAdvancedGaragePreview(
    const std::array<std::uint32_t, 16>& assembleWords);

std::string buildAdvancedGarageBuildLink(const std::array<std::optional<int>, 12>& compatibleBuildIds);

std::string formatAssembleWordsForDetail(const std::array<std::uint32_t, 16>& assembleWords);

}  // namespace ac6dm::ac
