#include "ac_preview_probe_service.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace ac6dm::ac {
namespace {

struct ExtractedPreview final {
    std::vector<std::uint8_t> bytes;
    std::string mediaType;
    std::string sourceHint;
};

std::uint32_t readLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes.at(offset)) |
           (static_cast<std::uint32_t>(bytes.at(offset + 1)) << 8U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 2)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 3)) << 24U);
}

std::uint32_t readBe32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes.at(offset)) << 24U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 1)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 2)) << 8U) |
           static_cast<std::uint32_t>(bytes.at(offset + 3));
}

bool startsWithAt(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::initializer_list<std::uint8_t> needle) {
    if (offset + needle.size() > bytes.size()) {
        return false;
    }
    std::size_t index = 0;
    for (const auto value : needle) {
        if (bytes[offset + index] != value) {
            return false;
        }
        ++index;
    }
    return true;
}

bool isStandaloneJpegMarker(std::uint8_t marker) {
    return marker == 0x01 || (marker >= 0xD0 && marker <= 0xD9);
}

bool isPlausibleJpegHeaderMarker(std::uint8_t marker) {
    return marker == 0xC0 || marker == 0xC1 || marker == 0xC2 || marker == 0xC4
        || marker == 0xDB || marker == 0xDD || marker == 0xDA
        || (marker >= 0xE0 && marker <= 0xEF);
}

bool hasQualifiedJpegAppHeader(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 11 >= bytes.size()) {
        return false;
    }
    const auto appMarker = bytes[offset + 3];
    if (appMarker == 0xE0) {
        return startsWithAt(bytes, offset + 6, {'J', 'F', 'I', 'F', 0x00});
    }
    if (appMarker == 0xE1) {
        return startsWithAt(bytes, offset + 6, {'E', 'x', 'i', 'f', 0x00, 0x00});
    }
    return false;
}

std::optional<ExtractedPreview> tryExtractPng(const std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint8_t, 8> kPngSignature{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    for (std::size_t offset = 0; offset + kPngSignature.size() + 8 < bytes.size(); ++offset) {
        if (!std::equal(kPngSignature.begin(), kPngSignature.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
            continue;
        }
        std::size_t cursor = offset + kPngSignature.size();
        bool sawIhdr = false;
        bool sawIend = false;
        while (cursor + 12 <= bytes.size()) {
            const auto chunkLength = readBe32(bytes, cursor);
            if (chunkLength > std::numeric_limits<std::uint32_t>::max() - 12U) {
                break;
            }
            const std::size_t totalChunkSize = static_cast<std::size_t>(chunkLength) + 12U;
            if (cursor + totalChunkSize > bytes.size()) {
                break;
            }
            const std::string chunkType{
                reinterpret_cast<const char*>(bytes.data() + static_cast<std::ptrdiff_t>(cursor + 4)),
                4};
            if (chunkType == "IHDR") {
                sawIhdr = true;
            }
            if (chunkType == "IEND") {
                sawIend = true;
                const auto end = cursor + totalChunkSize;
                return ExtractedPreview{
                    .bytes = std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(end)),
                    .mediaType = "image/png",
                    .sourceHint = "embedded-png@" + std::to_string(offset),
                };
            }
            cursor += totalChunkSize;
        }
        if (sawIhdr && sawIend) {
            break;
        }
    }
    return std::nullopt;
}

std::optional<ExtractedPreview> tryExtractJpeg(const std::vector<std::uint8_t>& bytes) {
    for (std::size_t offset = 0; offset + 5 < bytes.size(); ++offset) {
        if (!startsWithAt(bytes, offset, {0xFF, 0xD8, 0xFF})) {
            continue;
        }
        if (!hasQualifiedJpegAppHeader(bytes, offset)) {
            continue;
        }
        std::size_t cursor = offset + 2;
        const auto firstMarker = bytes[cursor + 1];
        if (!isPlausibleJpegHeaderMarker(firstMarker)) {
            continue;
        }
        while (cursor + 1 < bytes.size()) {
            if (bytes[cursor] != 0xFF) {
                break;
            }
            while (cursor + 1 < bytes.size() && bytes[cursor + 1] == 0xFF) {
                ++cursor;
            }
            if (cursor + 1 >= bytes.size()) {
                break;
            }
            const auto marker = bytes[cursor + 1];
            if (marker == 0xD9) {
                return ExtractedPreview{
                    .bytes = std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(cursor + 2)),
                    .mediaType = "image/jpeg",
                    .sourceHint = "embedded-jpeg@" + std::to_string(offset),
                };
            }
            if (isStandaloneJpegMarker(marker)) {
                cursor += 2;
                continue;
            }
            if (cursor + 3 >= bytes.size()) {
                break;
            }
            const auto segmentLength = static_cast<std::size_t>(bytes[cursor + 2] << 8U | bytes[cursor + 3]);
            if (segmentLength < 2) {
                break;
            }
            if (marker == 0xDA) {
                cursor += 2 + segmentLength;
                while (cursor + 1 < bytes.size()) {
                    if (bytes[cursor] != 0xFF) {
                        ++cursor;
                        continue;
                    }
                    if (bytes[cursor + 1] == 0x00) {
                        cursor += 2;
                        continue;
                    }
                    break;
                }
                continue;
            }
            cursor += 2 + segmentLength;
        }
    }
    return std::nullopt;
}

std::optional<ExtractedPreview> tryExtractBmp(const std::vector<std::uint8_t>& bytes) {
    for (std::size_t offset = 0; offset + 26 < bytes.size(); ++offset) {
        if (!startsWithAt(bytes, offset, {0x42, 0x4D})) {
            continue;
        }
        const auto declaredSize = readLe32(bytes, offset + 2);
        const auto pixelDataOffset = readLe32(bytes, offset + 10);
        const auto dibHeaderSize = readLe32(bytes, offset + 14);
        if (declaredSize < 54 || pixelDataOffset < 54 || dibHeaderSize < 40) {
            continue;
        }
        if (dibHeaderSize != 40 && dibHeaderSize != 52
            && dibHeaderSize != 56 && dibHeaderSize != 108 && dibHeaderSize != 124) {
            continue;
        }
        if (pixelDataOffset < 14 + dibHeaderSize) {
            continue;
        }
        const auto width = static_cast<std::uint32_t>(std::abs(static_cast<int>(readLe32(bytes, offset + 18))));
        const auto height = static_cast<std::uint32_t>(std::abs(static_cast<int>(readLe32(bytes, offset + 22))));
        const auto planes = static_cast<std::uint16_t>(bytes.at(offset + 26) | (bytes.at(offset + 27) << 8U));
        const auto bitsPerPixel = static_cast<std::uint16_t>(bytes.at(offset + 28) | (bytes.at(offset + 29) << 8U));
        const auto compression = readLe32(bytes, offset + 30);
        if (width == 0 || height == 0 || width > 4096 || height > 4096) {
            continue;
        }
        if (planes != 1 || (bitsPerPixel != 24 && bitsPerPixel != 32) || compression != 0) {
            continue;
        }
        const std::size_t end = offset + static_cast<std::size_t>(declaredSize);
        if (end > bytes.size()) {
            continue;
        }
        return ExtractedPreview{
            .bytes = std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(end)),
            .mediaType = "image/bmp",
            .sourceHint = "embedded-bmp@" + std::to_string(offset),
        };
    }
    return std::nullopt;
}

std::optional<ExtractedPreview> extractEmbeddedPreview(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    if (const auto png = tryExtractPng(bytes); png.has_value()) {
        return png;
    }
    if (const auto jpeg = tryExtractJpeg(bytes); jpeg.has_value()) {
        return jpeg;
    }
    if (const auto bmp = tryExtractBmp(bytes); bmp.has_value()) {
        return bmp;
    }
    return std::nullopt;
}

}  // namespace

preview::PreviewResolution probeAcPreview(const AcPreviewProbeInput& input) {
    if (!input.embeddedPreviewBytes.empty() && input.embeddedPreviewVerified) {
        return preview::makeNativeEmbeddedPreview(
            input.embeddedPreviewBytes,
            "ac.embedded-preview-probe",
            input.sourceHint.has_value() ? input.sourceHint : std::make_optional<std::string>("record-ref:" + input.recordRef),
            input.note,
            "image/png");
    }
    if (input.forensicPayloadScanEnabled) {
        if (const auto extracted = extractEmbeddedPreview(input.recordPayloadBytes); extracted.has_value()) {
            return preview::makeUnknownPreview(
                "ac.embedded-preview-scan",
                input.note.has_value()
                    ? input.note
                    : std::make_optional<std::string>("Forensic payload scan found a candidate image, but record-level preview provenance remains unqualified"),
                std::make_optional<std::string>(extracted->sourceHint));
        }
    }
    if (input.knownUnavailable) {
        return preview::makeUnavailablePreview(
            "ac.embedded-preview-probe",
            input.note.has_value() ? input.note : std::make_optional<std::string>("Native preview not present in current sample"),
            input.sourceHint);
    }
    return preview::makeUnknownPreview(
        "ac.embedded-preview-probe",
        input.note.has_value() ? input.note : std::make_optional<std::string>("Preview provenance pending qualification"),
        input.sourceHint);
}

}  // namespace ac6dm::ac
