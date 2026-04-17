#pragma once

#include "ac6dm/contracts/common_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ac6dm::preview {

enum class PreviewState {
    NativeEmbedded,
    DerivedRender,
    Unavailable,
    Unknown,
};

struct PreviewResolution final {
    PreviewState state{PreviewState::Unknown};
    std::string provenanceTag;
    std::optional<std::string> sourceHint;
    std::optional<std::string> note;
    contracts::PreviewHandle handle;
};

const char* toString(PreviewState state) noexcept;
PreviewState previewStateFromString(const std::string& value);

PreviewResolution makeNativeEmbeddedPreview(
    std::vector<std::uint8_t> imageBytes,
    std::string provenanceTag,
    std::optional<std::string> sourceHint = std::nullopt,
    std::optional<std::string> note = std::nullopt,
    std::string mediaType = "image/png");

PreviewResolution makeDerivedRenderPreview(
    std::vector<std::uint8_t> imageBytes,
    std::string provenanceTag,
    std::optional<std::string> sourceHint = std::nullopt,
    std::optional<std::string> note = std::nullopt,
    std::string mediaType = "image/png");

PreviewResolution makeUnavailablePreview(
    std::string provenanceTag,
    std::optional<std::string> note = std::make_optional<std::string>("No Preview"),
    std::optional<std::string> sourceHint = std::nullopt);

PreviewResolution makeUnknownPreview(
    std::string provenanceTag,
    std::optional<std::string> note = std::make_optional<std::string>("Preview provenance not qualified yet"),
    std::optional<std::string> sourceHint = std::nullopt);

void appendPreviewDetailFields(std::vector<contracts::DetailField>& detailFields, const PreviewResolution& resolution);

}  // namespace ac6dm::preview
