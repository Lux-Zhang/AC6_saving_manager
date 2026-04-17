#pragma once

#include "../preview/preview_models.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::ac {

struct AcPreviewProbeInput final {
    std::string assetId;
    std::string recordRef;
    std::vector<std::uint8_t> recordPayloadBytes;
    std::vector<std::uint8_t> embeddedPreviewBytes;
    bool embeddedPreviewVerified{false};
    bool knownUnavailable{false};
    bool forensicPayloadScanEnabled{false};
    std::optional<std::string> sourceHint;
    std::optional<std::string> note;
};

preview::PreviewResolution probeAcPreview(const AcPreviewProbeInput& input);

}  // namespace ac6dm::ac
