#pragma once

#include "emblem_domain.hpp"
#include "../preview/preview_models.hpp"

#include <vector>

class QImage;

namespace ac6dm::emblem {

preview::PreviewResolution buildEmblemPreview(const EmblemRecord& record);
QImage renderEmblemPreviewImage(const EmblemRecord& record);
std::vector<std::uint8_t> renderEmblemPreviewPng(const EmblemRecord& record);

}  // namespace ac6dm::emblem
