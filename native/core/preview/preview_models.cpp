#include "preview_models.hpp"

#include <QByteArray>
#include <QCryptographicHash>

#include <stdexcept>
#include <utility>

namespace ac6dm::preview {
namespace {

contracts::PreviewHandle makeHandle(
    PreviewState state,
    std::vector<std::uint8_t> imageBytes,
    std::optional<std::string> sourceHint,
    std::optional<std::string> note,
    std::optional<std::string> mediaType) {
    contracts::PreviewHandle handle;
    handle.kind = imageBytes.empty() ? contracts::PreviewKind::None : contracts::PreviewKind::Png;
    handle.label = "preview";
    handle.provenance = toString(state);
    handle.sourceHint = std::move(sourceHint);
    handle.note = std::move(note);
    handle.mediaType = std::move(mediaType);
    handle.imageBytes.assign(imageBytes.begin(), imageBytes.end());
    if (!imageBytes.empty()) {
        handle.sha256 = QCryptographicHash::hash(
                            QByteArray(reinterpret_cast<const char*>(imageBytes.data()), static_cast<int>(imageBytes.size())),
                            QCryptographicHash::Sha256)
                            .toHex()
                            .toStdString();
    }
    return handle;
}

PreviewResolution makeResolution(
    PreviewState state,
    std::vector<std::uint8_t> imageBytes,
    std::string provenanceTag,
    std::optional<std::string> sourceHint,
    std::optional<std::string> note,
    std::optional<std::string> mediaType) {
    PreviewResolution resolution;
    resolution.state = state;
    resolution.provenanceTag = std::move(provenanceTag);
    resolution.sourceHint = sourceHint;
    resolution.note = note;
    resolution.handle = makeHandle(state, std::move(imageBytes), std::move(sourceHint), std::move(note), std::move(mediaType));
    return resolution;
}

}  // namespace

const char* toString(PreviewState state) noexcept {
    switch (state) {
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

PreviewState previewStateFromString(const std::string& value) {
    if (value == "native_embedded") {
        return PreviewState::NativeEmbedded;
    }
    if (value == "derived_render") {
        return PreviewState::DerivedRender;
    }
    if (value == "unavailable") {
        return PreviewState::Unavailable;
    }
    if (value == "unknown") {
        return PreviewState::Unknown;
    }
    throw std::invalid_argument("Unknown preview state: " + value);
}

PreviewResolution makeNativeEmbeddedPreview(
    std::vector<std::uint8_t> imageBytes,
    std::string provenanceTag,
    std::optional<std::string> sourceHint,
    std::optional<std::string> note,
    std::string mediaType) {
    const bool hasImageBytes = !imageBytes.empty();
    return makeResolution(
        hasImageBytes ? PreviewState::NativeEmbedded : PreviewState::Unknown,
        std::move(imageBytes),
        std::move(provenanceTag),
        std::move(sourceHint),
        std::move(note),
        hasImageBytes ? std::make_optional(std::move(mediaType)) : std::optional<std::string>{});
}

PreviewResolution makeDerivedRenderPreview(
    std::vector<std::uint8_t> imageBytes,
    std::string provenanceTag,
    std::optional<std::string> sourceHint,
    std::optional<std::string> note,
    std::string mediaType) {
    const bool hasImageBytes = !imageBytes.empty();
    return makeResolution(
        hasImageBytes ? PreviewState::DerivedRender : PreviewState::Unavailable,
        std::move(imageBytes),
        std::move(provenanceTag),
        std::move(sourceHint),
        !hasImageBytes && !note.has_value() ? std::make_optional<std::string>("No Preview") : std::move(note),
        hasImageBytes ? std::make_optional(std::move(mediaType)) : std::optional<std::string>{});
}

PreviewResolution makeUnavailablePreview(
    std::string provenanceTag,
    std::optional<std::string> note,
    std::optional<std::string> sourceHint) {
    return makeResolution(
        PreviewState::Unavailable,
        {},
        std::move(provenanceTag),
        std::move(sourceHint),
        std::move(note),
        std::nullopt);
}

PreviewResolution makeUnknownPreview(
    std::string provenanceTag,
    std::optional<std::string> note,
    std::optional<std::string> sourceHint) {
    return makeResolution(
        PreviewState::Unknown,
        {},
        std::move(provenanceTag),
        std::move(sourceHint),
        std::move(note),
        std::nullopt);
}

void appendPreviewDetailFields(std::vector<contracts::DetailField>& detailFields, const PreviewResolution& resolution) {
    detailFields.push_back({"PreviewState", toString(resolution.state)});
    detailFields.push_back({"PreviewProvenance", resolution.provenanceTag});
    if (resolution.sourceHint.has_value()) {
        detailFields.push_back({"PreviewSource", *resolution.sourceHint});
    }
    if (resolution.note.has_value()) {
        detailFields.push_back({"PreviewNote", *resolution.note});
    }
}

}  // namespace ac6dm::preview
