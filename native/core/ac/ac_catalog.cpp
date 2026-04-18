#include "ac_catalog.hpp"
#include "ac_preview_support.hpp"

#include <QString>

#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

namespace ac6dm::ac {

namespace {

struct KnownAcSlotFile final {
    const char* filename;
    contracts::SourceBucket sourceBucket;
};

struct ParsedAcRecord final {
    std::uint64_t beginOffset{0};
    std::uint64_t endOffsetExclusive{0};
    std::uint32_t firstDesignMagicOffset{0};
    std::vector<std::uint8_t> rawBytes;
};

struct ParsedAcContainer final {
    std::filesystem::path filePath;
    std::array<std::uint8_t, 16> iv{};
    std::vector<std::uint8_t> plainBytes;
    std::uint32_t capacityField{0};
    std::uint32_t firstRecordDesignOffset{0};
    std::uint32_t recordCountField{0};
    std::vector<std::uint8_t> footerBytes;
    std::vector<ParsedAcRecord> records;
    bool qualified{false};
    std::string qualificationNote;
};

struct RecordBlock final {
    std::string name;
    std::vector<std::uint8_t> payload;
};

struct AcDisplayMetadata final {
    std::string archiveName;
    std::string machineName;
    std::string shareCode;
    std::size_t blockCount{0};
    std::optional<std::array<std::uint32_t, 16>> assembleWords;
};

class Cursor final {
public:
    explicit Cursor(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

    std::size_t tell() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    std::vector<std::uint8_t> read(const std::size_t size) {
        if (offset_ + size > bytes_.size()) {
            throw AcQualificationError("AC record block parse exceeded payload boundary");
        }
        std::vector<std::uint8_t> slice(
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
        offset_ += size;
        return slice;
    }

    template <typename T>
    T readPod() {
        auto slice = read(sizeof(T));
        T value{};
        std::memcpy(&value, slice.data(), sizeof(T));
        return value;
    }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_{0};
};

constexpr std::array<unsigned char, 16> kAcDataKey = {
    0xB1, 0x56, 0x87, 0x9F, 0x13, 0x48, 0x97, 0x98,
    0x70, 0x05, 0xC4, 0x87, 0x00, 0xAE, 0xF8, 0x79
};
constexpr std::array<char, 15> kBeginMarker = {
    '-', '-', '-', '-', ' ', 'b', 'e', 'g', 'i', 'n', ' ', '-', '-', '-', '-'
};
constexpr std::array<char, 15> kEndMarker = {
    '-', '-', '-', '-', ' ', ' ', 'e', 'n', 'd', ' ', ' ', '-', '-', '-', '-'
};
constexpr std::array<char, 4> kAsmcMagic = {'A', 'S', 'M', 'C'};
// Current save forensics still only qualify USER_DATA002..006 as the editable/share AC record containers.
// Real decrypted samples show USER_DATA008 uses a different multi-block shape
// (Summary/Account/Result/Assemble/Emblem/PilotName/NamePlate/GradeRank) and is not a Design-based AC library.
constexpr std::array<KnownAcSlotFile, 5> kKnownAcSlotFiles{{
    {"USER_DATA002", contracts::SourceBucket::User1},
    {"USER_DATA003", contracts::SourceBucket::User2},
    {"USER_DATA004", contracts::SourceBucket::User3},
    {"USER_DATA005", contracts::SourceBucket::User4},
    {"USER_DATA006", contracts::SourceBucket::Share},
}};

constexpr std::uint64_t kEncryptedIvBytes = 16;
constexpr std::uint64_t kContainerHeaderBytes = 20;
constexpr std::uint64_t kContainerChecksumBytes = 16;
constexpr std::uint64_t kContainerReservedTailBytes = 12;
constexpr std::uint64_t kContainerFooterBytes = kContainerChecksumBytes + kContainerReservedTailBytes;
constexpr std::uint64_t kRecordDelimiterBytes = 32;
constexpr int kUserBucketCount = 4;
constexpr int kMaxRecordsPerUserBucket = 40;
constexpr std::size_t kShareCodeOffset = 0x40;
constexpr std::size_t kArchiveNameOffset = 0xA2;
constexpr std::size_t kMachineNameOffset = 0xE2;
constexpr std::size_t kAsmcHeaderBytes = 16U;
constexpr std::array<char, 4> kAsmcContainerMagic = {'A', 'S', 'M', 'C'};
constexpr std::uint32_t kAsmcContainerSentinel = 0x00291222U;

std::string readFixedCString(Cursor& cursor, const std::size_t size = 16) {
    const auto raw = cursor.read(size);
    const auto terminator = std::find(raw.begin(), raw.end(), static_cast<std::uint8_t>(0));
    return std::string(raw.begin(), terminator == raw.end() ? raw.end() : terminator);
}

std::string decodeUtf16LeNullTerminated(const std::vector<std::uint8_t>& payload) {
    if ((payload.size() % 2U) != 0U) {
        return {};
    }
    QString text;
    for (std::size_t index = 0; index < payload.size(); index += 2) {
        const char16_t codeUnit = static_cast<char16_t>(payload[index] | (payload[index + 1] << 8));
        if (codeUnit == 0) {
            return text.toStdString();
        }
        text.append(QChar(codeUnit));
    }
    return {};
}

bool isLikelyHumanTextCodeUnit(const char16_t codeUnit) {
    return codeUnit == u' '
        || (codeUnit >= 0x0021 && codeUnit <= 0x007E)
        || (codeUnit >= 0x00A1 && codeUnit <= 0x024F)
        || (codeUnit >= 0x3040 && codeUnit <= 0x30FF)
        || (codeUnit >= 0x3400 && codeUnit <= 0x9FFF)
        || (codeUnit >= 0xAC00 && codeUnit <= 0xD7AF)
        || (codeUnit >= 0xF900 && codeUnit <= 0xFAFF)
        || (codeUnit >= 0xFF01 && codeUnit <= 0xFF5E);
}

std::string decodeValidatedUtf16LeNullTerminated(
    const std::vector<std::uint8_t>& payload,
    const std::size_t maxCodeUnits = 64U) {
    if ((payload.size() % 2U) != 0U) {
        return {};
    }

    QString text;
    std::size_t codeUnits = 0;
    for (std::size_t index = 0; index < payload.size() && codeUnits < maxCodeUnits; index += 2, ++codeUnits) {
        const char16_t codeUnit = static_cast<char16_t>(payload[index] | (payload[index + 1] << 8));
        if (codeUnit == 0) {
            return text.isEmpty() ? std::string{} : text.toStdString();
        }
        if (!isLikelyHumanTextCodeUnit(codeUnit)) {
            return {};
        }
        text.append(QChar(codeUnit));
    }
    return {};
}

std::string decodeAsciiNullTerminated(const std::vector<std::uint8_t>& payload) {
    const auto terminator = std::find(payload.begin(), payload.end(), static_cast<std::uint8_t>(0));
    const auto end = terminator == payload.end() ? payload.end() : terminator;
    const auto printable = std::all_of(payload.begin(), end, [](const auto value) {
        return value == 0 || (value >= 32 && value <= 126);
    });
    if (!printable) {
        return {};
    }
    return std::string(payload.begin(), end);
}

std::string readUtf16LeStringAt(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::size_t maxCodeUnits = 64) {
    if (offset >= bytes.size() || offset + 1 >= bytes.size()) {
        return {};
    }
    return decodeValidatedUtf16LeNullTerminated(
        std::vector<std::uint8_t>(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.end()),
        maxCodeUnits);
}

bool looksLikeShareCode(const std::string& value) {
    if (value.size() < 11U || value.size() > 14U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
        return std::isdigit(ch) != 0 || (ch >= 'A' && ch <= 'Z');
    });
}

std::vector<RecordBlock> parseRecordBlocks(const std::vector<std::uint8_t>& recordBytes) {
    Cursor cursor(recordBytes);
    const auto beginName = readFixedCString(cursor);
    const auto beginSize = cursor.readPod<std::uint32_t>();
    std::ignore = cursor.readPod<std::uint32_t>();
    std::ignore = cursor.readPod<std::uint64_t>();
    if (beginName != "---- begin ----" || beginSize != 0U) {
        throw AcQualificationError("AC record is missing the expected begin delimiter");
    }

    std::vector<RecordBlock> blocks;
    while (cursor.remaining() > 0) {
        const auto name = readFixedCString(cursor);
        const auto size = cursor.readPod<std::uint32_t>();
        std::ignore = cursor.readPod<std::uint32_t>();
        std::ignore = cursor.readPod<std::uint64_t>();
        if (name == "----  end  ----") {
            if (size != 0U) {
                throw AcQualificationError("AC record end delimiter has non-zero size");
            }
            break;
        }
        blocks.push_back(RecordBlock{
            .name = name,
            .payload = cursor.read(size),
        });
    }
    return blocks;
}

std::optional<std::vector<std::uint8_t>> inflateAsmcPayload(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < kAsmcHeaderBytes
        || !std::equal(kAsmcContainerMagic.begin(), kAsmcContainerMagic.end(), payload.begin())) {
        return std::nullopt;
    }

    const auto readLe32At = [&](const std::size_t offset) -> std::uint32_t {
        std::uint32_t value = 0;
        std::memcpy(&value, payload.data() + static_cast<std::ptrdiff_t>(offset), sizeof(value));
        return value;
    };
    const auto sentinel = readLe32At(4);
    const auto compressedSize = readLe32At(8);
    const auto uncompressedSize = readLe32At(12);
    if (sentinel != kAsmcContainerSentinel
        || compressedSize == 0U
        || uncompressedSize == 0U
        || static_cast<std::size_t>(kAsmcHeaderBytes + compressedSize) > payload.size()) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> inflated(uncompressedSize, 0);
    uLongf actualSize = static_cast<uLongf>(inflated.size());
    const auto result = ::uncompress(
        inflated.data(),
        &actualSize,
        payload.data() + static_cast<std::ptrdiff_t>(kAsmcHeaderBytes),
        compressedSize);
    if (result != Z_OK) {
        return std::nullopt;
    }

    inflated.resize(static_cast<std::size_t>(actualSize));
    if (actualSize != static_cast<uLongf>(uncompressedSize)) {
        return std::nullopt;
    }
    return inflated;
}

std::optional<std::array<std::uint32_t, 16>> parseAssembleWords(const std::vector<std::uint8_t>& payload) {
    if (payload.size() != (16U * sizeof(std::uint32_t))) {
        return std::nullopt;
    }
    std::array<std::uint32_t, 16> words{};
    std::memcpy(words.data(), payload.data(), payload.size());
    return words;
}

AcDisplayMetadata extractDisplayMetadata(const std::vector<std::uint8_t>& recordBytes) {
    AcDisplayMetadata metadata;
    std::vector<RecordBlock> blocks;
    try {
        blocks = parseRecordBlocks(recordBytes);
        metadata.blockCount = blocks.size();
    } catch (const std::exception&) {
        metadata.blockCount = 0;
    }

    const auto findPayloadIn = [](const std::vector<RecordBlock>& searchBlocks, const std::string& blockName)
        -> const std::vector<std::uint8_t>* {
        const auto it = std::find_if(searchBlocks.begin(), searchBlocks.end(), [&](const auto& block) {
            return block.name == blockName;
        });
        return it == searchBlocks.end() ? nullptr : &it->payload;
    };
    const auto readTextBlockFrom = [&](const std::vector<RecordBlock>& searchBlocks, const std::string& blockName)
        -> std::string {
        const auto* payload = findPayloadIn(searchBlocks, blockName);
        if (payload == nullptr) {
            return {};
        }
        const auto utf16 = decodeValidatedUtf16LeNullTerminated(*payload);
        if (!utf16.empty()) {
            return utf16;
        }
        return decodeAsciiNullTerminated(*payload);
    };
    const auto readTextBlock = [&](const std::string& blockName) -> std::string {
        return readTextBlockFrom(blocks, blockName);
    };

    if (const auto* designPayload = findPayloadIn(blocks, "Design"); designPayload != nullptr) {
        if (const auto inflated = inflateAsmcPayload(*designPayload); inflated.has_value()) {
            try {
                const auto designBlocks = parseRecordBlocks(*inflated);
                metadata.archiveName = readTextBlockFrom(designBlocks, "DataName");
                metadata.machineName = readTextBlockFrom(designBlocks, "AcName");
                const auto candidate = readTextBlockFrom(designBlocks, "UgcID");
                if (looksLikeShareCode(candidate)) {
                    metadata.shareCode = candidate;
                }
                if (const auto* assemblePayload = findPayloadIn(designBlocks, "Assemble");
                    assemblePayload != nullptr) {
                    metadata.assembleWords = parseAssembleWords(*assemblePayload);
                }
            } catch (const std::exception&) {
            }
        }

        if (metadata.archiveName.empty()) {
            metadata.archiveName = readUtf16LeStringAt(*designPayload, kArchiveNameOffset);
        }
        if (metadata.machineName.empty()) {
            metadata.machineName = readUtf16LeStringAt(*designPayload, kMachineNameOffset);
        }
        if (metadata.shareCode.empty()) {
            const auto candidate = readUtf16LeStringAt(*designPayload, kShareCodeOffset, 16);
            if (looksLikeShareCode(candidate)) {
                metadata.shareCode = candidate;
            }
        }
    }

    if (metadata.archiveName.empty()) {
        metadata.archiveName = readTextBlock("DataName");
    }
    if (metadata.machineName.empty()) {
        metadata.machineName = readTextBlock("AcName");
    }
    if (metadata.shareCode.empty()) {
        metadata.shareCode = readTextBlock("UgcID");
    }
    return metadata;
}

std::string twoDigitIndex(const int value) {
    char buffer[16] = {0};
    std::snprintf(buffer, sizeof(buffer), "%02d", value + 1);
    return std::string(buffer);
}

std::string sourceBucketDisplayLabel(const contracts::SourceBucket bucket) {
    switch (bucket) {
    case contracts::SourceBucket::User1:
        return "User 1";
    case contracts::SourceBucket::User2:
        return "User 2";
    case contracts::SourceBucket::User3:
        return "User 3";
    case contracts::SourceBucket::User4:
        return "User 4";
    case contracts::SourceBucket::Share:
        return "Share";
    }
    return "Share";
}

std::string makeDisplayName(const contracts::SourceBucket bucket, const int recordIndex) {
    return sourceBucketDisplayLabel(bucket) + " AC " + twoDigitIndex(recordIndex);
}

std::string buildAcDisplayLabel(
    const std::string& archiveName,
    const std::string& machineName,
    const std::string& shareCode,
    const contracts::SourceBucket bucket,
    const int recordIndex) {
    std::vector<std::string> lines;
    lines.push_back("Archive: " + (!archiveName.empty() ? archiveName : sourceBucketDisplayLabel(bucket) + " Archive " + twoDigitIndex(recordIndex)));
    lines.push_back("AC: " + (!machineName.empty() ? machineName : sourceBucketDisplayLabel(bucket) + " AC " + twoDigitIndex(recordIndex)));
    lines.push_back("Share code: " + (!shareCode.empty() ? shareCode : "-"));
    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << lines[index];
    }
    return stream.str();
}

std::string makeRecordLabel(const contracts::SourceBucket bucket, const int recordIndex) {
    return sourceBucketDisplayLabel(bucket) + " / AC " + twoDigitIndex(recordIndex);
}

std::string makeTargetLabel(const int ownerSlot, const int recordIndex) {
    const auto bucket = ownerSlot == 0
        ? contracts::SourceBucket::User1
        : ownerSlot == 1
            ? contracts::SourceBucket::User2
            : ownerSlot == 2
                ? contracts::SourceBucket::User3
                : contracts::SourceBucket::User4;
    return makeRecordLabel(bucket, recordIndex);
}

std::string fallbackArchiveName(const contracts::SourceBucket bucket, const int recordIndex) {
    return sourceBucketDisplayLabel(bucket) + " Archive " + twoDigitIndex(recordIndex);
}

std::string fallbackMachineName(const contracts::SourceBucket bucket, const int recordIndex) {
    return sourceBucketDisplayLabel(bucket) + " AC " + twoDigitIndex(recordIndex);
}

const KnownAcSlotFile* findKnownSlot(const contracts::SourceBucket bucket) {
    for (const auto& entry : kKnownAcSlotFiles) {
        if (entry.sourceBucket == bucket) {
            return &entry;
        }
    }
    return nullptr;
}

contracts::SourceBucket sourceBucketFromOwnerSlot(const int ownerSlot) {
    switch (ownerSlot) {
    case 0:
        return contracts::SourceBucket::User1;
    case 1:
        return contracts::SourceBucket::User2;
    case 2:
        return contracts::SourceBucket::User3;
    case 3:
        return contracts::SourceBucket::User4;
    default:
        throw AcQualificationError("AC owner slot index out of range");
    }
}

std::string stableIdFor(const std::vector<std::uint8_t>& payload) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto value : payload) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << "fnv1a64-" << std::hex << hash;
    return stream.str();
}

std::string logicalBucketLabel(const contracts::SourceBucket bucket) {
    return contracts::toString(bucket);
}

std::string assetIdFor(const contracts::SourceBucket bucket, const int recordIndex) {
    return "ac:" + contracts::toString(bucket) + ":" + std::to_string(recordIndex);
}

const AcCatalogRecord* findRecordByAssetId(const AcCatalogSnapshot& snapshot, const std::string& assetId) {
    for (const auto& record : snapshot.records) {
        if (record.assetId == assetId) {
            return &record;
        }
    }
    return nullptr;
}

std::filesystem::path resolveContainerFile(
    const std::filesystem::path& unpackedSaveDir,
    const contracts::SourceBucket sourceBucket) {
    const auto* knownSlot = findKnownSlot(sourceBucket);
    if (knownSlot == nullptr) {
        throw AcQualificationError("AC source bucket has no mapped USER_DATA file");
    }
    return unpackedSaveDir / knownSlot->filename;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw AcQualificationError("Unable to open AC payload file: " + path.generic_string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string output;
    output.reserve(bytes.size() * 2U);
    for (const auto value : bytes) {
        output.push_back(kHexDigits[(value >> 4U) & 0x0F]);
        output.push_back(kHexDigits[value & 0x0F]);
    }
    return output;
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw AcQualificationError("Unable to write AC payload file: " + path.generic_string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw AcQualificationError("Failed while writing AC payload file: " + path.generic_string());
    }
}

std::uint32_t readLe32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    if (offset + sizeof(std::uint32_t) > bytes.size()) {
        throw AcQualificationError("AC container header read exceeded payload boundary");
    }
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + static_cast<std::ptrdiff_t>(offset), sizeof(value));
    return value;
}

void writeLe32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    if (offset + sizeof(std::uint32_t) > bytes.size()) {
        throw AcQualificationError("AC container header write exceeded payload boundary");
    }
    std::memcpy(bytes.data() + static_cast<std::ptrdiff_t>(offset), &value, sizeof(value));
}

std::vector<std::uint8_t> runAesCbc(
    const std::vector<std::uint8_t>& payload,
    const std::array<std::uint8_t, 16>& iv,
    const bool encrypt) {
    EVP_CIPHER_CTX* rawCtx = EVP_CIPHER_CTX_new();
    if (rawCtx == nullptr) {
        throw AcQualificationError("Failed to create AC AES context");
    }
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(rawCtx, &EVP_CIPHER_CTX_free);

    int ok = encrypt
        ? EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv.data())
        : EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv.data());
    if (ok != 1) {
        throw AcQualificationError("Failed to initialize AC AES-CBC context");
    }
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);

    std::vector<std::uint8_t> output(payload.size() + 16);
    int outLen1 = 0;
    ok = encrypt
        ? EVP_EncryptUpdate(ctx.get(), output.data(), &outLen1, payload.data(), static_cast<int>(payload.size()))
        : EVP_DecryptUpdate(ctx.get(), output.data(), &outLen1, payload.data(), static_cast<int>(payload.size()));
    if (ok != 1) {
        throw AcQualificationError(encrypt ? "AC AES-CBC encryption failed" : "AC AES-CBC decryption failed");
    }
    int outLen2 = 0;
    ok = encrypt
        ? EVP_EncryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2)
        : EVP_DecryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2);
    if (ok != 1) {
        throw AcQualificationError(encrypt ? "AC AES-CBC encryption finalization failed"
                                           : "AC AES-CBC decryption finalization failed");
    }

    output.resize(static_cast<std::size_t>(outLen1 + outLen2));
    return output;
}

std::vector<std::uint8_t> decryptContainerPayload(
    const std::vector<std::uint8_t>& encryptedBytes,
    std::array<std::uint8_t, 16>* outIv) {
    if (encryptedBytes.size() <= kEncryptedIvBytes) {
        throw AcQualificationError("AC container is too small to contain IV + ciphertext");
    }
    std::array<std::uint8_t, 16> iv{};
    std::copy_n(encryptedBytes.begin(), iv.size(), iv.begin());
    auto cipherBytes = std::vector<std::uint8_t>(encryptedBytes.begin() + static_cast<std::ptrdiff_t>(kEncryptedIvBytes),
                                                 encryptedBytes.end());
    if (cipherBytes.size() % 16 != 0) {
        throw AcQualificationError("AC container ciphertext length must be a multiple of 16 bytes");
    }
    if (outIv != nullptr) {
        *outIv = iv;
    }
    return runAesCbc(cipherBytes, iv, false);
}

std::vector<std::uint8_t> encryptContainerPayload(
    const std::vector<std::uint8_t>& plainBytes,
    const std::array<std::uint8_t, 16>& iv) {
    if (plainBytes.size() % 16 != 0) {
        throw AcQualificationError("AC plaintext length must remain aligned to AES block size");
    }
    auto encrypted = runAesCbc(plainBytes, iv, true);
    std::vector<std::uint8_t> output;
    output.insert(output.end(), iv.begin(), iv.end());
    output.insert(output.end(), encrypted.begin(), encrypted.end());
    return output;
}

bool matchesMarker(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::array<char, 15>& marker) {
    if (offset + marker.size() > bytes.size()) {
        return false;
    }
    for (std::size_t index = 0; index < marker.size(); ++index) {
        if (bytes[offset + index] != static_cast<std::uint8_t>(marker[index])) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint64_t> findAllMarkers(
    const std::vector<std::uint8_t>& bytes,
    const std::array<char, 15>& marker,
    const std::size_t startOffset,
    const std::size_t endExclusive) {
    std::vector<std::uint64_t> positions;
    if (endExclusive <= startOffset || endExclusive > bytes.size()) {
        return positions;
    }
    for (std::size_t offset = startOffset; offset + marker.size() <= endExclusive; ++offset) {
        if (matchesMarker(bytes, offset, marker)) {
            positions.push_back(static_cast<std::uint64_t>(offset));
        }
    }
    return positions;
}

std::optional<std::uint32_t> findFirstMagicOffset(
    const std::vector<std::uint8_t>& bytes,
    const std::array<char, 4>& magic) {
    if (bytes.size() < magic.size()) {
        return std::nullopt;
    }
    for (std::size_t offset = 0; offset + magic.size() <= bytes.size(); ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < magic.size(); ++index) {
            if (bytes[offset + index] != static_cast<std::uint8_t>(magic[index])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return static_cast<std::uint32_t>(offset);
        }
    }
    return std::nullopt;
}

ParsedAcRecord parseRecordSpan(
    const std::vector<std::uint8_t>& plainBytes,
    const std::uint64_t beginOffset,
    const std::uint64_t endOffsetExclusive) {
    if (endOffsetExclusive <= beginOffset || endOffsetExclusive > plainBytes.size()) {
        throw AcQualificationError("AC record span is outside container boundary");
    }
    ParsedAcRecord record;
    record.beginOffset = beginOffset;
    record.endOffsetExclusive = endOffsetExclusive;
    record.rawBytes.assign(
        plainBytes.begin() + static_cast<std::ptrdiff_t>(beginOffset),
        plainBytes.begin() + static_cast<std::ptrdiff_t>(endOffsetExclusive));
    record.firstDesignMagicOffset = findFirstMagicOffset(record.rawBytes, kAsmcMagic).value_or(0U);
    return record;
}

ParsedAcContainer parseContainerFile(const std::filesystem::path& path) {
    ParsedAcContainer container;
    container.filePath = path;
    auto encryptedBytes = readBytes(path);
    container.plainBytes = decryptContainerPayload(encryptedBytes, &container.iv);
    if (container.plainBytes.size() < kContainerHeaderBytes + kContainerFooterBytes) {
        throw AcQualificationError("AC container plaintext is too small");
    }
    container.capacityField = readLe32(container.plainBytes, 0);
    container.firstRecordDesignOffset = readLe32(container.plainBytes, 12);
    container.recordCountField = readLe32(container.plainBytes, 16);
    container.footerBytes.assign(
        container.plainBytes.end() - static_cast<std::ptrdiff_t>(kContainerFooterBytes),
        container.plainBytes.end());

    const auto footerStart = container.plainBytes.size() - static_cast<std::size_t>(kContainerFooterBytes);
    const auto beginOffsets = findAllMarkers(container.plainBytes, kBeginMarker, kContainerHeaderBytes, footerStart);
    const auto endOffsets = findAllMarkers(container.plainBytes, kEndMarker, kContainerHeaderBytes, footerStart);
    const auto parsedCount = std::min(beginOffsets.size(), endOffsets.size());

    bool countsMatch = beginOffsets.size() == endOffsets.size()
        && beginOffsets.size() == static_cast<std::size_t>(container.recordCountField);
    bool spansValid = true;
    for (std::size_t index = 0; index < parsedCount; ++index) {
        const auto beginOffset = beginOffsets[index];
        const auto endOffset = endOffsets[index];
        const auto nextRecordOffset = index + 1U < beginOffsets.size()
            ? beginOffsets[index + 1U]
            : endOffset + kRecordDelimiterBytes;
        if (endOffset < beginOffset || nextRecordOffset < endOffset + kRecordDelimiterBytes || nextRecordOffset > footerStart) {
            spansValid = false;
            break;
        }
        container.records.push_back(parseRecordSpan(container.plainBytes, beginOffset, nextRecordOffset));
    }

    container.qualified = countsMatch && spansValid;
    if (!container.qualified) {
        std::ostringstream note;
        note << "headerCount=" << container.recordCountField
             << ", beginCount=" << beginOffsets.size()
             << ", endCount=" << endOffsets.size();
        container.qualificationNote = note.str();
    }
    return container;
}

std::vector<std::uint8_t> rebuildPlaintext(
    const ParsedAcContainer& container,
    const std::vector<std::vector<std::uint8_t>>& records) {
    if (container.plainBytes.size() < kContainerHeaderBytes + kContainerFooterBytes) {
        throw AcQualificationError("AC container template is too small for rebuild");
    }
    auto output = container.plainBytes;
    const auto footerStart = output.size() - static_cast<std::size_t>(kContainerFooterBytes);
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(kContainerHeaderBytes),
              output.begin() + static_cast<std::ptrdiff_t>(footerStart),
              std::uint8_t{0});

    std::size_t writeOffset = static_cast<std::size_t>(kContainerHeaderBytes);
    for (const auto& record : records) {
        if (writeOffset + record.size() > footerStart) {
            throw AcQualificationError("AC container does not have enough free space for rebuilt records");
        }
        std::copy(record.begin(), record.end(), output.begin() + static_cast<std::ptrdiff_t>(writeOffset));
        writeOffset += record.size();
    }

    // Real saves contain container header fields that are not a pure function of the
    // current record bytes. Preserve the original values and only update record count.
    writeLe32(output, 0, container.capacityField);
    writeLe32(output, 12, container.firstRecordDesignOffset);
    writeLe32(output, 16, static_cast<std::uint32_t>(records.size()));

    const auto checksumStart = output.size() - static_cast<std::size_t>(kContainerFooterBytes);
    unsigned int digestSize = 0;
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    const auto ok = EVP_Digest(
        output.data() + 4,
        output.size() - static_cast<std::size_t>(4) - static_cast<std::size_t>(kContainerFooterBytes),
        digest.data(),
        &digestSize,
        EVP_md5(),
        nullptr);
    if (ok != 1 || digestSize != kContainerChecksumBytes) {
        throw AcQualificationError("AC checksum recomputation failed");
    }
    std::copy_n(
        digest.begin(),
        static_cast<std::size_t>(kContainerChecksumBytes),
        output.begin() + static_cast<std::ptrdiff_t>(checksumStart));
    return output;
}

std::uint8_t expectedCategoryForBucket(const contracts::SourceBucket bucket) {
    switch (bucket) {
    case contracts::SourceBucket::User1:
        return 0x01;
    case contracts::SourceBucket::User2:
        return 0x02;
    case contracts::SourceBucket::User3:
        return 0x03;
    case contracts::SourceBucket::User4:
        return 0x04;
    case contracts::SourceBucket::Share:
        return 0x07;
    }
    return 0x07;
}

std::vector<std::uint8_t> patchPayloadForTargetBucket(
    const std::vector<std::uint8_t>& payload,
    const contracts::SourceBucket targetBucket) {
    auto patched = payload;
    Cursor cursor(patched);
    const auto beginName = readFixedCString(cursor);
    const auto beginSize = cursor.readPod<std::uint32_t>();
    std::ignore = cursor.readPod<std::uint32_t>();
    std::ignore = cursor.readPod<std::uint64_t>();
    if (beginName != "---- begin ----" || beginSize != 0U) {
        throw AcQualificationError("AC record payload does not begin with the expected begin marker");
    }

    while (cursor.remaining() > 0) {
        const auto blockOffset = cursor.tell();
        const auto name = readFixedCString(cursor);
        const auto size = cursor.readPod<std::uint32_t>();
        std::ignore = cursor.readPod<std::uint32_t>();
        std::ignore = cursor.readPod<std::uint64_t>();
        if (name == "----  end  ----") {
            if (size != 0U) {
                throw AcQualificationError("AC record end delimiter has non-zero size");
            }
            break;
        }
        if (name == "Category" && size == 1U) {
            patched[blockOffset + static_cast<std::size_t>(kRecordDelimiterBytes)] = expectedCategoryForBucket(targetBucket);
            break;
        }
        std::ignore = cursor.read(size);
    }
    return patched;
}

int currentRecordCountForBucket(const AcCatalogSnapshot& snapshot, const contracts::SourceBucket bucket) {
    return static_cast<int>(std::count_if(snapshot.records.begin(), snapshot.records.end(), [&](const auto& record) {
        return record.sourceBucket == bucket;
    }));
}

std::string makeTargetBucketLabel(const int ownerSlot) {
    return sourceBucketDisplayLabel(sourceBucketFromOwnerSlot(ownerSlot));
}

std::vector<contracts::ImportTargetChoiceDto> buildTargetChoices(const AcCatalogSnapshot& snapshot) {
    std::vector<contracts::ImportTargetChoiceDto> choices;
    choices.reserve(static_cast<std::size_t>(kUserBucketCount));
    for (int ownerSlot = 0; ownerSlot < kUserBucketCount; ++ownerSlot) {
        const auto bucket = sourceBucketFromOwnerSlot(ownerSlot);
        const int currentCount = currentRecordCountForBucket(snapshot, bucket);
        if (currentCount >= kMaxRecordsPerUserBucket) {
            continue;
        }
        choices.push_back({
            .code = ownerSlot,
            .label = makeTargetBucketLabel(ownerSlot),
            .note = "Next write slot: " + makeTargetLabel(ownerSlot, currentCount),
        });
    }
    return choices;
}

std::optional<int> findSuggestedTargetSlot(const AcCatalogSnapshot& snapshot) {
    for (int ownerSlot = 0; ownerSlot < kUserBucketCount; ++ownerSlot) {
        const auto bucket = sourceBucketFromOwnerSlot(ownerSlot);
        const int currentCount = currentRecordCountForBucket(snapshot, bucket);
        if (currentCount < kMaxRecordsPerUserBucket) {
            return ownerSlot;
        }
    }
    return std::nullopt;
}

std::string detailProvenanceForRecordMap() {
    return "ac.record-ref.aes-cbc.begin-end.v1";
}

ParsedAcContainer readContainerForRecord(
    const std::filesystem::path& unpackedSaveDir,
    const AcCatalogRecord& record) {
    const auto path = unpackedSaveDir / record.recordRef.containerFile;
    return parseContainerFile(path);
}

std::vector<std::uint8_t> payloadForRecordIndex(
    const ParsedAcContainer& container,
    const int recordIndex) {
    if (recordIndex < 0 || recordIndex >= static_cast<int>(container.records.size())) {
        throw AcQualificationError("AC record index is outside parsed container range");
    }
    return container.records[static_cast<std::size_t>(recordIndex)].rawBytes;
}

std::string validateTargetSlot(const AcCatalogSnapshot& snapshot, const int ownerSlot) {
    if (ownerSlot < 0 || ownerSlot >= kUserBucketCount) {
        return "Target user slot must be User 1, User 2, User 3, or User 4.";
    }
    const int currentCount = currentRecordCountForBucket(snapshot, sourceBucketFromOwnerSlot(ownerSlot));
    if (currentCount >= kMaxRecordsPerUserBucket) {
        return "The target user AC library is full and cannot accept another record.";
    }
    return {};
}

}  // namespace

std::string serializeRecordRef(const AcRecordRef& recordRef) {
    std::ostringstream stream;
    stream << recordRef.containerFile.generic_string()
           << "/record/" << recordRef.recordIndex
           << "?bucket=" << recordRef.logicalBucket
           << "&offset=" << recordRef.byteOffset
           << "&size=" << recordRef.byteLength
           << "&stable=" << recordRef.stableId
           << "&qualified=" << (recordRef.qualified ? 1 : 0);
    if (recordRef.ownerSlot.has_value()) {
        stream << "&ownerSlot=" << *recordRef.ownerSlot;
    }
    return stream.str();
}

std::string describeTargetSlotCode(const int targetUserSlotIndex) {
    if (targetUserSlotIndex < 0 || targetUserSlotIndex >= kUserBucketCount) {
        throw AcQualificationError("AC target user bucket is out of range");
    }
    return makeTargetBucketLabel(targetUserSlotIndex);
}

AcCatalogSnapshot buildProvisionalCatalogSnapshot(const std::filesystem::path& unpackedSaveDir) {
    AcCatalogSnapshot snapshot;
    bool allQualified = true;
    std::size_t discoveredFileCount = 0;

    for (const auto& entry : kKnownAcSlotFiles) {
        const auto filePath = unpackedSaveDir / entry.filename;
        if (!std::filesystem::exists(filePath)) {
            continue;
        }
        ++discoveredFileCount;

        ParsedAcContainer container;
        try {
            container = parseContainerFile(filePath);
        } catch (const std::exception&) {
            allQualified = false;
            continue;
        }

        if (!container.qualified) {
            allQualified = false;
        }
        if (entry.sourceBucket != contracts::SourceBucket::Share
            && static_cast<int>(container.records.size()) > kMaxRecordsPerUserBucket) {
            allQualified = false;
            container.qualified = false;
            container.qualificationNote = "record count exceeds known AC capacity of 40";
        }

        for (int recordIndex = 0; recordIndex < static_cast<int>(container.records.size()); ++recordIndex) {
            const auto& parsedRecord = container.records[static_cast<std::size_t>(recordIndex)];
            AcDisplayMetadata metadata;
            try {
                metadata = extractDisplayMetadata(parsedRecord.rawBytes);
            } catch (const std::exception&) {
                metadata = {};
            }

            AcCatalogRecord record{
                .assetId = assetIdFor(entry.sourceBucket, recordIndex),
                .sourceBucket = entry.sourceBucket,
                .slotIndex = contracts::slotIndexOf(entry.sourceBucket),
                .recordRef = AcRecordRef{
                    .containerFile = filePath.filename(),
                    .ownerSlot = contracts::slotIndexOf(entry.sourceBucket),
                    .logicalBucket = logicalBucketLabel(entry.sourceBucket),
                    .recordIndex = recordIndex,
                    .byteOffset = parsedRecord.beginOffset,
                    .byteLength = parsedRecord.endOffsetExclusive - parsedRecord.beginOffset,
                    .stableId = stableIdFor(parsedRecord.rawBytes),
                    .qualified = container.qualified,
                },
                .byteSize = static_cast<std::uintmax_t>(parsedRecord.rawBytes.size()),
            };
            snapshot.records.push_back(record);

            contracts::CatalogItemDto dto;
            dto.assetId = record.assetId;
            dto.assetKind = contracts::AssetKind::Ac;
            dto.sourceBucket = record.sourceBucket;
            dto.slotIndex = record.slotIndex;
            dto.archiveName = metadata.archiveName.empty()
                ? fallbackArchiveName(record.sourceBucket, recordIndex)
                : metadata.archiveName;
            dto.machineName = metadata.machineName.empty()
                ? fallbackMachineName(record.sourceBucket, recordIndex)
                : metadata.machineName;
            dto.shareCode = metadata.shareCode;
            dto.displayName = buildAcDisplayLabel(
                dto.archiveName,
                dto.machineName,
                dto.shareCode,
                record.sourceBucket,
                recordIndex);
            dto.writeCapability = record.sourceBucket == contracts::SourceBucket::Share
                ? contracts::WriteCapability::ReadOnly
                : contracts::WriteCapability::Editable;
            dto.recordRef = serializeRecordRef(record.recordRef);
            dto.detailProvenance = detailProvenanceForRecordMap();
            dto.slotLabel = makeRecordLabel(record.sourceBucket, recordIndex);
            dto.origin = record.sourceBucket == contracts::SourceBucket::Share
                ? contracts::AssetOrigin::Share
                : contracts::AssetOrigin::User;
            dto.editable = record.sourceBucket != contracts::SourceBucket::Share;
            dto.sourceLabel = sourceBucketDisplayLabel(record.sourceBucket);
            dto.description = "AC encrypted container record built from USER_DATA00X AES-CBC begin/end records, with target-bucket Category patching and entry checksum rewrite during import.";
            dto.tags = {"ac", "record-ref-qualified", "aes-cbc-begin-end"};
            if (metadata.assembleWords.has_value()) {
                dto.acPreview = tryBuildAdvancedGaragePreview(*metadata.assembleWords);
                dto.previewState = dto.acPreview.has_value()
                    ? contracts::PreviewState::DerivedRender
                    : contracts::PreviewState::Unknown;
                dto.preview.provenance = dto.acPreview.has_value()
                    ? "advanced-garage.compatibility.partial"
                    : "advanced-garage.compatibility.unavailable";
            } else {
                dto.previewState = contracts::PreviewState::Unavailable;
                dto.preview.provenance = "advanced-garage.compatibility.no-assemble";
            }
            dto.detailFields.push_back({"AssetKind", contracts::toString(dto.assetKind)});
            dto.detailFields.push_back({"SourceBucket", contracts::toString(dto.sourceBucket)});
            dto.detailFields.push_back({"WriteCapability", contracts::toString(dto.writeCapability)});
            dto.detailFields.push_back({"RecordRef", dto.recordRef});
            dto.detailFields.push_back({"DetailProvenance", dto.detailProvenance});
            dto.detailFields.push_back({"ArchiveName", dto.archiveName});
            dto.detailFields.push_back({"AcName", dto.machineName});
            dto.detailFields.push_back({"ShareCode", dto.shareCode.empty() ? "-" : dto.shareCode});
            dto.detailFields.push_back({"ContainerFile", record.recordRef.containerFile.generic_string()});
            dto.detailFields.push_back({"StableId", record.recordRef.stableId});
            dto.detailFields.push_back({"RecordIndex", std::to_string(record.recordRef.recordIndex)});
            dto.detailFields.push_back({"RecordByteOffset", std::to_string(record.recordRef.byteOffset)});
            dto.detailFields.push_back({"RecordByteLength", std::to_string(record.recordRef.byteLength)});
            dto.detailFields.push_back({"ContainerRecordCount", std::to_string(container.records.size())});
            dto.detailFields.push_back({"ContainerHeaderCount", std::to_string(container.recordCountField)});
            dto.detailFields.push_back({"FirstRecordDesignOffset", std::to_string(container.firstRecordDesignOffset)});
            dto.detailFields.push_back({"RecordBlockCount", std::to_string(metadata.blockCount)});
            if (metadata.assembleWords.has_value()) {
                dto.detailFields.push_back({"AssembleWords", formatAssembleWordsForDetail(*metadata.assembleWords)});
            }
            if (dto.acPreview.has_value()) {
                dto.detailFields.push_back({"BuildLinkCompatible", dto.acPreview->buildLinkCompatible ? "true" : "false"});
                dto.detailFields.push_back({"BuildLinkUrl", dto.acPreview->buildLinkUrl.empty() ? "-" : dto.acPreview->buildLinkUrl});
                dto.detailFields.push_back({"AcPreviewNote", dto.acPreview->note});
            }
            if (!container.qualified) {
                dto.detailFields.push_back({"QualificationNote", container.qualificationNote});
            }
            snapshot.catalog.push_back(dto);
        }
    }

    snapshot.recordMapQualified = discoveredFileCount > 0 && allQualified;
    snapshot.editableClonePatch = AcEditableClonePatchV1{
        .version = "ac-location-copy-v2",
        .qualified = snapshot.recordMapQualified,
        .note = snapshot.recordMapQualified
            ? "begin/end clone with bucket-category patch and entry checksum rewrite"
            : "AC encrypted begin/end record model is not qualified for current sample",
    };

    if (!snapshot.recordMapQualified) {
        for (auto& record : snapshot.records) {
            record.recordRef.qualified = false;
        }
        for (auto& item : snapshot.catalog) {
            item.writeCapability = contracts::WriteCapability::LockedPendingGate;
            item.editable = false;
            item.tags.push_back("record-map-unqualified");
            item.detailFields.push_back({"RecordMapQualified", "false"});
            item.detailFields.push_back({"EditableClonePatchV1", snapshot.editableClonePatch.note});
        }
    } else {
        for (auto& item : snapshot.catalog) {
            item.detailFields.push_back({"RecordMapQualified", "true"});
            item.detailFields.push_back({"EditableClonePatchV1", snapshot.editableClonePatch.version});
        }
    }

    return snapshot;
}

std::vector<AcContainerDebugSummary> debugSummarizeContainers(const std::filesystem::path& unpackedSaveDir) {
    std::vector<AcContainerDebugSummary> summaries;
    for (const auto& entry : kKnownAcSlotFiles) {
        const auto filePath = unpackedSaveDir / entry.filename;
        if (!std::filesystem::exists(filePath)) {
            continue;
        }

        const auto container = parseContainerFile(filePath);
        AcContainerDebugSummary summary;
        summary.containerFile = filePath.filename().generic_string();
        summary.capacityField = container.capacityField;
        summary.firstRecordDesignOffset = container.firstRecordDesignOffset;
        summary.recordCountField = container.recordCountField;
        summary.parsedRecordCount = static_cast<int>(container.records.size());
        summary.qualified = container.qualified;
        summary.qualificationNote = container.qualificationNote;
        summary.footerHex = bytesToHex(container.footerBytes);
        summary.records.reserve(container.records.size());

        for (int recordIndex = 0; recordIndex < static_cast<int>(container.records.size()); ++recordIndex) {
            const auto& parsedRecord = container.records[static_cast<std::size_t>(recordIndex)];
            AcDisplayMetadata metadata;
            try {
                metadata = extractDisplayMetadata(parsedRecord.rawBytes);
            } catch (...) {
                metadata = {};
            }

            summary.records.push_back({
                .recordIndex = recordIndex,
                .stableId = stableIdFor(parsedRecord.rawBytes),
                .byteOffset = parsedRecord.beginOffset,
                .byteLength = parsedRecord.endOffsetExclusive - parsedRecord.beginOffset,
                .archiveName = metadata.archiveName,
                .machineName = metadata.machineName,
                .shareCode = metadata.shareCode,
            });
        }

        summaries.push_back(std::move(summary));
    }
    return summaries;
}

contracts::ImportPlanDto buildShareAcImportPlan(
    const AcCatalogSnapshot& snapshot,
    const std::string& assetId,
    std::optional<int> targetUserSlotIndex) {
    contracts::ImportPlanDto plan;
    plan.title = "Import selected AC to user slot";
    plan.sourceAssetIds = {assetId};
    plan.mode = "ac-share-copy";
    plan.requiresExplicitTargetUserSlotSelection = true;
    plan.dryRunRequired = false;
    plan.shadowWorkspaceRequired = true;
    plan.postWriteReadbackRequired = true;
    plan.targetChoices = buildTargetChoices(snapshot);

    const auto* record = findRecordByAssetId(snapshot, assetId);
    if (record == nullptr) {
        plan.blockers.push_back("The selected AC record was not found.");
        plan.targetSlot = "Choose a target user slot";
        return plan;
    }

    if (!snapshot.recordMapQualified || !snapshot.editableClonePatch.qualified) {
        plan.mode = "blocked";
        plan.summary = "AC import is blocked because the record-level parser or editable clone patch is not qualified.";
        plan.blockers.push_back("ac record-level parser not qualified");
        plan.blockers.push_back("AcEditableClonePatchV1 not qualified");
    }

    if (record->sourceBucket != contracts::SourceBucket::Share) {
        plan.blockers.push_back("Only Share AC records can be imported into a user slot.");
    }

    if (!targetUserSlotIndex.has_value()) {
        const auto suggested = findSuggestedTargetSlot(snapshot).value_or(0);
        plan.suggestedTargetUserSlotIndex = suggested;
        plan.targetSlot = "Choose a target user slot";
        plan.summary = plan.blockers.empty()
            ? "Choose the destination user AC slot to continue."
            : "AC import plan is blocked.";
        plan.warnings.push_back("This import uses a begin/end record-level editable clone and patches the target-bucket Category plus entry checksum before writeback. The selected user bucket receives the next empty AC slot.");
        plan.operations.push_back({
            .title = "Recommended user slot",
            .target = describeTargetSlotCode(suggested),
            .result = "suggested",
            .note = "The default recommendation is the first user slot that can still append another AC record."
        });
        if (plan.targetChoices.empty()) {
            plan.blockers.push_back("All user AC slots are full.");
            plan.summary = "AC import plan is blocked.";
        }
        return plan;
    }

    const auto targetError = validateTargetSlot(snapshot, *targetUserSlotIndex);
    if (!targetError.empty()) {
        plan.blockers.push_back(targetError);
        plan.targetSlot = "Invalid slot";
    } else {
        const auto targetBucket = sourceBucketFromOwnerSlot(*targetUserSlotIndex);
        const int currentCount = currentRecordCountForBucket(snapshot, targetBucket);
        plan.suggestedTargetUserSlotIndex = *targetUserSlotIndex;
        plan.targetSlot = makeTargetLabel(*targetUserSlotIndex, currentCount);
        plan.operations.push_back({
            .title = "Append Share AC record",
            .target = plan.targetSlot,
            .result = plan.blockers.empty() ? "pending" : "blocked",
            .note = "The selected Share AC will be appended after the current last AC in the target user slot.",
        });
    }

    plan.warnings.push_back("This write runs inside the staging workspace. Readback must pass before the opened save is replaced.");
    if (plan.blockers.empty()) {
        plan.summary = "AC import plan is ready. Execution will append the Share AC to the selected user slot.";
    } else if (plan.summary.empty()) {
        plan.summary = "AC import plan is blocked.";
    }
    return plan;
}

contracts::ImportPlanDto buildExchangeAcImportPlan(
    const AcCatalogSnapshot& snapshot,
    std::optional<int> targetUserSlotIndex) {
    std::ignore = snapshot;
    contracts::ImportPlanDto plan;
    plan.title = "Import AC file to user slot";
    plan.mode = "ac-exchange-import";
    plan.requiresExplicitTargetUserSlotSelection = true;
    plan.dryRunRequired = false;
    plan.shadowWorkspaceRequired = true;
    plan.postWriteReadbackRequired = true;
    plan.targetChoices = buildTargetChoices(snapshot);

    if (!snapshot.recordMapQualified || !snapshot.editableClonePatch.qualified) {
        plan.mode = "blocked";
        plan.summary = "AC file import is blocked because the record-level parser or editable clone patch is not qualified.";
        plan.blockers.push_back("ac record-level parser not qualified");
        plan.blockers.push_back("AcEditableClonePatchV1 not qualified");
        return plan;
    }

    if (!targetUserSlotIndex.has_value()) {
        const auto suggested = findSuggestedTargetSlot(snapshot).value_or(0);
        plan.suggestedTargetUserSlotIndex = suggested;
        plan.targetSlot = "Choose a target user slot";
        plan.summary = "Choose the destination user AC slot to continue.";
        plan.warnings.push_back("This import uses a begin/end raw record clone. The selected user bucket receives the next empty AC slot.");
        return plan;
    }

    const auto targetError = validateTargetSlot(snapshot, *targetUserSlotIndex);
    if (!targetError.empty()) {
        plan.blockers.push_back(targetError);
        plan.targetSlot = "Invalid slot";
    } else {
        const int currentCount = currentRecordCountForBucket(snapshot, sourceBucketFromOwnerSlot(*targetUserSlotIndex));
        plan.suggestedTargetUserSlotIndex = *targetUserSlotIndex;
        plan.targetSlot = makeTargetLabel(*targetUserSlotIndex, currentCount);
    }

    if (plan.blockers.empty()) {
        plan.summary = "AC file import plan is ready. Execution will append the exchange-package record to the selected user slot and patch the target-bucket Category plus entry checksum.";
        plan.operations.push_back({
            .title = "AC exchange payload append",
            .target = plan.targetSlot,
            .result = "pending",
            .note = "The exchange-package AC record will be appended after the current last AC in the selected user slot.",
        });
    } else {
        plan.summary = "AC file import plan is blocked.";
    }
    return plan;
}

std::vector<std::uint8_t> readRecordPayload(
    const std::filesystem::path& unpackedSaveDir,
    const std::string& assetId,
    const AcCatalogSnapshot& snapshot) {
    const auto* record = findRecordByAssetId(snapshot, assetId);
    if (record == nullptr) {
        throw AcQualificationError("AC assetId not found: " + assetId);
    }
    const auto container = readContainerForRecord(unpackedSaveDir, *record);
    return payloadForRecordIndex(container, record->recordRef.recordIndex);
}

void applyPayloadToUserSlot(
    const std::filesystem::path& unpackedSaveDir,
    const std::vector<std::uint8_t>& payload,
    int targetUserSlotIndex,
    std::vector<std::uint8_t>* outWrittenPayload) {
    if (!matchesMarker(payload, 0, kBeginMarker)) {
        throw AcQualificationError("AC record payload does not begin with the expected begin marker");
    }
    if (targetUserSlotIndex < 0 || targetUserSlotIndex >= kUserBucketCount) {
        throw AcQualificationError("AC target user bucket is out of range");
    }
    const auto targetBucket = sourceBucketFromOwnerSlot(targetUserSlotIndex);
    const auto targetPath = resolveContainerFile(unpackedSaveDir, targetBucket);
    auto container = parseContainerFile(targetPath);
    if (!container.qualified) {
        throw AcQualificationError("Target AC container is not qualified for record-level writeback");
    }

    auto rebuiltRecords = std::vector<std::vector<std::uint8_t>>{};
    rebuiltRecords.reserve(container.records.size() + 1U);
    for (const auto& record : container.records) {
        rebuiltRecords.push_back(record.rawBytes);
    }

    if (static_cast<int>(rebuiltRecords.size()) >= kMaxRecordsPerUserBucket) {
        throw AcQualificationError("Target AC bucket is already at capacity");
    }
    const auto normalizedPayload = patchPayloadForTargetBucket(payload, targetBucket);
    rebuiltRecords.push_back(normalizedPayload);
    if (outWrittenPayload != nullptr) {
        *outWrittenPayload = normalizedPayload;
    }

    const auto plainBytes = rebuildPlaintext(container, rebuiltRecords);
    const auto encryptedBytes = encryptContainerPayload(plainBytes, container.iv);
    writeBytes(targetPath, encryptedBytes);
}

std::vector<std::uint8_t> applyShareAcPayloadCopy(
    const std::filesystem::path& unpackedSaveDir,
    const AcCatalogSnapshot& snapshot,
    const std::string& assetId,
    int targetUserSlotIndex) {
    const auto plan = buildShareAcImportPlan(snapshot, assetId, targetUserSlotIndex);
    if (!plan.blockers.empty()) {
        throw AcQualificationError(plan.blockers.front());
    }
    const auto payload = readRecordPayload(unpackedSaveDir, assetId, snapshot);
    std::vector<std::uint8_t> writtenPayload;
    applyPayloadToUserSlot(unpackedSaveDir, payload, targetUserSlotIndex, &writtenPayload);
    return writtenPayload;
}

AcWriteReadback verifyShareAcPayloadReadback(
    const std::filesystem::path& unpackedSaveDir,
    int targetUserSlotIndex,
    const std::vector<std::uint8_t>& expectedPayload,
    std::string sourceAssetId) {
    if (targetUserSlotIndex < 0 || targetUserSlotIndex >= kUserBucketCount) {
        throw AcQualificationError("AC target user bucket is out of range");
    }
    const auto targetBucket = sourceBucketFromOwnerSlot(targetUserSlotIndex);
    const auto targetPath = resolveContainerFile(unpackedSaveDir, targetBucket);
    const auto container = parseContainerFile(targetPath);
    if (container.records.empty()) {
        throw AcQualificationError("AC readback target slot is outside the parsed record range");
    }

    const int targetRecordIndex = static_cast<int>(container.records.size()) - 1;
    const auto& parsedRecord = container.records.back();
    const AcRecordRef recordRef{
        .containerFile = targetPath.filename(),
        .ownerSlot = targetUserSlotIndex,
        .logicalBucket = logicalBucketLabel(targetBucket),
        .recordIndex = targetRecordIndex,
        .byteOffset = parsedRecord.beginOffset,
        .byteLength = parsedRecord.endOffsetExclusive - parsedRecord.beginOffset,
        .stableId = stableIdFor(parsedRecord.rawBytes),
        .qualified = container.qualified,
    };

    return AcWriteReadback{
        .sourceAssetId = std::move(sourceAssetId),
        .targetUserSlotIndex = targetUserSlotIndex,
        .targetRecordRef = recordRef,
        .expectedPayloadBytes = expectedPayload.size(),
        .actualPayloadBytes = parsedRecord.rawBytes.size(),
        .payloadMatches = parsedRecord.rawBytes == expectedPayload,
    };
}

}  // namespace ac6dm::ac
