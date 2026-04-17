#include "emblem_domain.hpp"
#include "emblem_preview_service.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QString>

#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <set>
#include <sstream>

namespace ac6dm::emblem {

namespace {

constexpr std::array<unsigned char, 16> kUserData007Key = {
    0xB1, 0x56, 0x87, 0x9F, 0x13, 0x48, 0x97, 0x98,
    0x70, 0x05, 0xC4, 0x87, 0x00, 0xAE, 0xF8, 0x79
};
constexpr std::int32_t kUserDataFileSentinel = 0x00291222;
constexpr std::int16_t kGroupNodeMask = 0x3F00;
constexpr std::int16_t kGroupNodeValue = 0x3F00;

class Cursor final {
public:
    explicit Cursor(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

    std::size_t tell() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    std::vector<std::uint8_t> read(std::size_t size) {
        if (offset_ + size > bytes_.size()) {
            throw EmblemFormatError("Cursor read beyond payload boundary");
        }
        std::vector<std::uint8_t> slice(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
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

std::vector<std::uint8_t> toVector(const QByteArray& payload) {
    return std::vector<std::uint8_t>(payload.begin(), payload.end());
}

std::string readFixedCString(Cursor& cursor, std::size_t size = 16) {
    const auto raw = cursor.read(size);
    const auto terminator = std::find(raw.begin(), raw.end(), static_cast<std::uint8_t>(0));
    if (terminator == raw.end()) {
        throw EmblemFormatError("Fixed string missing null terminator");
    }
    return std::string(raw.begin(), terminator);
}

std::vector<std::uint8_t> writeFixedCString(const std::string& value, std::size_t size = 16) {
    if (value.size() >= size) {
        throw EmblemFormatError("String too long for fixed string field");
    }
    std::vector<std::uint8_t> result(size, 0);
    std::copy(value.begin(), value.end(), result.begin());
    return result;
}

std::string decodeUtf16LeNullTerminated(const std::vector<std::uint8_t>& payload) {
    if (payload.size() % 2 != 0) {
        throw EmblemFormatError("UTF-16LE payload must have even length");
    }
    QString result;
    for (std::size_t index = 0; index < payload.size(); index += 2) {
        const char16_t codeUnit = static_cast<char16_t>(payload[index] | (payload[index + 1] << 8));
        if (codeUnit == 0) {
            return result.toStdString();
        }
        result.append(QChar(codeUnit));
    }
    throw EmblemFormatError("UTF-16LE payload missing null terminator");
}

std::vector<std::uint8_t> encodeUtf16LeNullTerminated(const std::string& value) {
    const QString text = QString::fromStdString(value);
    QByteArray bytes(reinterpret_cast<const char*>(text.utf16()), text.size() * 2);
    bytes.append('\0');
    bytes.append('\0');
    return toVector(bytes);
}

QDateTime filetimeToDateTime(std::uint64_t filetime) {
    static constexpr qint64 kWindowsToUnixEpoch100Ns = 116444736000000000LL;
    const qint64 msSinceEpoch = static_cast<qint64>((filetime - kWindowsToUnixEpoch100Ns) / 10000ULL);
    return QDateTime::fromMSecsSinceEpoch(msSinceEpoch, Qt::UTC);
}

std::uint64_t dateTimeToFiletime(const QDateTime& dateTime) {
    static constexpr qint64 kWindowsToUnixEpoch100Ns = 116444736000000000LL;
    const auto utc = dateTime.toUTC();
    return static_cast<std::uint64_t>(utc.toMSecsSinceEpoch()) * 10000ULL + kWindowsToUnixEpoch100Ns;
}

std::uint64_t packSystemTime(const QDateTime& dateTime) {
    const auto utc = dateTime.toUTC();
    const auto time = utc.time();
    const auto date = utc.date();
    std::uint64_t cst = static_cast<std::uint64_t>(time.second() & 0x3F);
    cst = static_cast<std::uint64_t>(time.minute() & 0x3F) | (cst << 0x06);
    cst = static_cast<std::uint64_t>(time.hour() & 0x1F) | (cst << 0x05);
    cst = static_cast<std::uint64_t>(date.day() & 0x1F) | (cst << 0x05);
    cst = static_cast<std::uint64_t>(date.dayOfWeek() % 7) | (cst << 0x03);
    cst = static_cast<std::uint64_t>(date.month() & 0x0F) | (cst << 0x04);
    cst = static_cast<std::uint64_t>(time.msec() & 0x03FF) | (cst << 0x0A);
    cst = static_cast<std::uint64_t>(date.year() & 0x0FFF) | (cst << 0x0C);
    return cst;
}

std::vector<std::uint8_t> appendBytes(std::vector<std::uint8_t> buffer, const std::vector<std::uint8_t>& payload) {
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    return buffer;
}

template <typename T>
void appendPod(std::vector<std::uint8_t>& buffer, const T& value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    buffer.insert(buffer.end(), raw, raw + sizeof(T));
}

std::vector<std::uint8_t> serializeGroupData(const GroupData& data) {
    std::vector<std::uint8_t> buffer;
    appendPod(buffer, data.decalId);
    appendPod(buffer, data.posX);
    appendPod(buffer, data.posY);
    appendPod(buffer, data.scaleX);
    appendPod(buffer, data.scaleY);
    appendPod(buffer, data.angle);
    buffer.push_back(data.red);
    buffer.push_back(data.green);
    buffer.push_back(data.blue);
    buffer.push_back(data.alpha);
    appendPod(buffer, data.maskMode);
    appendPod(buffer, data.pad);
    return buffer;
}

GroupData deserializeGroupData(Cursor& cursor) {
    GroupData data;
    data.decalId = cursor.readPod<std::int16_t>();
    data.posX = cursor.readPod<std::int16_t>();
    data.posY = cursor.readPod<std::int16_t>();
    data.scaleX = cursor.readPod<std::int16_t>();
    data.scaleY = cursor.readPod<std::int16_t>();
    data.angle = cursor.readPod<std::int16_t>();
    data.red = cursor.readPod<std::uint8_t>();
    data.green = cursor.readPod<std::uint8_t>();
    data.blue = cursor.readPod<std::uint8_t>();
    data.alpha = cursor.readPod<std::uint8_t>();
    data.maskMode = cursor.readPod<std::int16_t>();
    data.pad = cursor.readPod<std::int16_t>();
    return data;
}

bool isGroupNode(std::int16_t decalId) {
    return (decalId & kGroupNodeMask) == kGroupNodeValue;
}

std::vector<std::uint8_t> serializeGroup(const EmblemGroup& group) {
    auto buffer = serializeGroupData(group.data);
    if (!group.children.empty()) {
        const auto childCount = static_cast<std::int32_t>(group.children.size());
        appendPod(buffer, childCount);
        for (const auto& child : group.children) {
            const auto childBytes = serializeGroup(child);
            buffer.insert(buffer.end(), childBytes.begin(), childBytes.end());
        }
    }
    return buffer;
}

EmblemGroup deserializeGroup(Cursor& cursor) {
    EmblemGroup group;
    group.data = deserializeGroupData(cursor);
    if (isGroupNode(group.data.decalId)) {
        const auto childCount = cursor.readPod<std::int32_t>();
        for (int index = 0; index < childCount; ++index) {
            group.children.push_back(deserializeGroup(cursor));
        }
    }
    return group;
}

std::vector<std::uint8_t> serializeImage(const EmblemImage& image) {
    std::vector<std::uint8_t> buffer;
    const std::int16_t marker = 0;
    const std::int16_t layerCount = static_cast<std::int16_t>(image.layers.size());
    appendPod(buffer, marker);
    appendPod(buffer, layerCount);
    for (const auto& layer : image.layers) {
        const std::int16_t layerMarker = 3;
        const std::int16_t layerVersion = 1;
        appendPod(buffer, layerMarker);
        appendPod(buffer, layerVersion);
        const auto groupBytes = serializeGroup(layer.group);
        buffer.insert(buffer.end(), groupBytes.begin(), groupBytes.end());
    }
    return buffer;
}

EmblemImage deserializeImage(const std::vector<std::uint8_t>& payload) {
    Cursor cursor(payload);
    const auto marker = cursor.readPod<std::int16_t>();
    const auto layerCount = cursor.readPod<std::int16_t>();
    if (marker != 0) {
        throw EmblemFormatError("Unexpected image header marker");
    }
    EmblemImage image;
    for (int index = 0; index < layerCount; ++index) {
        const auto layerMarker = cursor.readPod<std::int16_t>();
        const auto layerVersion = cursor.readPod<std::int16_t>();
        if (layerMarker != 3 || layerVersion != 1) {
            throw EmblemFormatError("Unexpected emblem layer header");
        }
        image.layers.push_back(EmblemLayer{.group = deserializeGroup(cursor)});
    }
    if (cursor.remaining() != 0) {
        throw EmblemFormatError("Trailing bytes remained after emblem image parse");
    }
    return image;
}

std::vector<EmblemBlock> deserializeBlockContainer(const std::vector<std::uint8_t>& payload) {
    Cursor cursor(payload);
    std::vector<EmblemBlock> blocks;
    bool firstBlock = true;
    while (cursor.remaining() > 0) {
        const auto name = readFixedCString(cursor);
        const auto size = cursor.readPod<std::uint32_t>();
        const auto unk = cursor.readPod<std::uint32_t>();
        const auto pad = cursor.readPod<std::uint64_t>();
        if (unk != 0 || pad != 0) {
            throw EmblemFormatError("Unexpected block control fields");
        }
        if (firstBlock) {
            if (name != "---- begin ----" || size != 0) {
                throw EmblemFormatError("Missing EMBC begin delimiter");
            }
            firstBlock = false;
            continue;
        }
        if (name == "----  end  ----") {
            if (size != 0) {
                throw EmblemFormatError("EMBC end delimiter must have zero size");
            }
            break;
        }
        blocks.push_back(EmblemBlock{
            .name = name,
            .payload = cursor.read(size),
        });
    }
    if (cursor.remaining() != 0) {
        throw EmblemFormatError("Trailing bytes remained after EMBC block parse");
    }
    return blocks;
}

std::vector<std::uint8_t> serializeBlockContainer(const std::vector<EmblemBlock>& blocks) {
    std::vector<std::uint8_t> buffer = writeFixedCString("---- begin ----");
    appendPod(buffer, std::uint32_t{0});
    appendPod(buffer, std::uint32_t{0});
    appendPod(buffer, std::uint64_t{0});
    for (const auto& block : blocks) {
        auto nameBytes = writeFixedCString(block.name);
        buffer.insert(buffer.end(), nameBytes.begin(), nameBytes.end());
        appendPod(buffer, static_cast<std::uint32_t>(block.payload.size()));
        appendPod(buffer, std::uint32_t{0});
        appendPod(buffer, std::uint64_t{0});
        buffer.insert(buffer.end(), block.payload.begin(), block.payload.end());
    }
    auto endBytes = writeFixedCString("----  end  ----");
    buffer.insert(buffer.end(), endBytes.begin(), endBytes.end());
    appendPod(buffer, std::uint32_t{0});
    appendPod(buffer, std::uint32_t{0});
    appendPod(buffer, std::uint64_t{0});
    return buffer;
}


UserDataFile parseUserDataFile(Cursor& cursor) {
    const auto magicBytes = cursor.read(4);
    const auto sentinel = cursor.readPod<std::int32_t>();
    const auto deflatedSize = cursor.readPod<std::int32_t>();
    const auto inflatedSize = cursor.readPod<std::int32_t>();
    if (sentinel != kUserDataFileSentinel) {
        throw EmblemFormatError("Unexpected USER_DATA007 file sentinel");
    }
    const auto compressed = cursor.read(static_cast<std::size_t>(deflatedSize));
    uLongf outputSize = static_cast<uLongf>(inflatedSize);
    std::vector<std::uint8_t> inflated(static_cast<std::size_t>(inflatedSize));
    const auto zResult = ::uncompress(inflated.data(), &outputSize, compressed.data(), static_cast<uLong>(compressed.size()));
    if (zResult != Z_OK) {
        throw EmblemFormatError("Failed to inflate USER_DATA007 file payload");
    }
    inflated.resize(static_cast<std::size_t>(outputSize));
    if (static_cast<int>(inflated.size()) != inflatedSize) {
        throw EmblemFormatError("Inflated payload length mismatch");
    }
    return UserDataFile{
        .fileType = std::string(magicBytes.begin(), magicBytes.end()),
        .data = std::move(inflated),
    };
}

std::vector<std::uint8_t> serializeUserDataFile(const UserDataFile& file) {
    if (file.fileType.size() != 4) {
        throw EmblemFormatError("USER_DATA007 file type must be exactly four ASCII characters");
    }
    uLongf compressedBound = ::compressBound(static_cast<uLong>(file.data.size()));
    std::vector<std::uint8_t> compressed(compressedBound);
    auto zResult = ::compress(compressed.data(), &compressedBound, file.data.data(), static_cast<uLong>(file.data.size()));
    if (zResult != Z_OK) {
        throw EmblemFormatError("Failed to deflate USER_DATA007 file payload");
    }
    compressed.resize(static_cast<std::size_t>(compressedBound));

    std::vector<std::uint8_t> buffer;
    buffer.insert(buffer.end(), file.fileType.begin(), file.fileType.end());
    appendPod(buffer, kUserDataFileSentinel);
    appendPod(buffer, static_cast<std::int32_t>(compressed.size()));
    appendPod(buffer, static_cast<std::int32_t>(file.data.size()));
    buffer.insert(buffer.end(), compressed.begin(), compressed.end());
    return buffer;
}

std::vector<std::uint8_t> runAesCbc(const std::vector<std::uint8_t>& payload, const std::array<std::uint8_t, 16>& iv, bool encrypt) {
    EVP_CIPHER_CTX* rawCtx = EVP_CIPHER_CTX_new();
    if (rawCtx == nullptr) {
        throw EmblemFormatError("Failed to create AES context");
    }
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(rawCtx, &EVP_CIPHER_CTX_free);

    const auto* cipher = EVP_aes_128_cbc();
    int ok = encrypt
        ? EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, kUserData007Key.data(), iv.data())
        : EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, kUserData007Key.data(), iv.data());
    if (ok != 1) {
        throw EmblemFormatError("Failed to initialize AES-CBC context");
    }
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);

    std::vector<std::uint8_t> output(payload.size() + 16);
    int outLen1 = 0;
    ok = encrypt
        ? EVP_EncryptUpdate(ctx.get(), output.data(), &outLen1, payload.data(), static_cast<int>(payload.size()))
        : EVP_DecryptUpdate(ctx.get(), output.data(), &outLen1, payload.data(), static_cast<int>(payload.size()));
    if (ok != 1) {
        throw EmblemFormatError(encrypt ? "AES-CBC encryption failed" : "AES-CBC decryption failed");
    }
    int outLen2 = 0;
    ok = encrypt
        ? EVP_EncryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2)
        : EVP_DecryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2);
    if (ok != 1) {
        throw EmblemFormatError(encrypt ? "AES-CBC encryption finalization failed" : "AES-CBC decryption finalization failed");
    }
    output.resize(static_cast<std::size_t>(outLen1 + outLen2));
    return output;
}

std::vector<std::uint8_t> decryptAesCbc(const std::vector<std::uint8_t>& payload, const std::array<std::uint8_t, 16>& iv) {
    return runAesCbc(payload, iv, false);
}

std::vector<std::uint8_t> encryptAesCbc(const std::vector<std::uint8_t>& payload, const std::array<std::uint8_t, 16>& iv) {
    return runAesCbc(payload, iv, true);
}

UserDataContainer deserializePlaintext(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < 40) {
        throw EmblemFormatError("USER_DATA007 payload is too small");
    }
    Cursor cursor(payload);
    UserDataContainer container;
    auto ivBytes = cursor.read(16);
    std::copy(ivBytes.begin(), ivBytes.end(), container.iv.begin());
    const auto sizeField = cursor.readPod<std::int32_t>();
    container.unk1 = cursor.readPod<std::int32_t>();
    container.unk2 = cursor.readPod<std::int32_t>();
    container.unk3 = cursor.readPod<std::int32_t>();
    container.unk4 = cursor.readPod<std::int32_t>();
    const auto fileCount = cursor.readPod<std::int32_t>();
    for (int index = 0; index < fileCount; ++index) {
        container.files.push_back(parseUserDataFile(cursor));
    }
    const auto extraCount = cursor.readPod<std::int32_t>();
    for (int index = 0; index < extraCount; ++index) {
        container.extraFiles.push_back(parseUserDataFile(cursor));
    }

    const std::size_t checksumOffset = static_cast<std::size_t>(4 + sizeField);
    if (cursor.tell() > checksumOffset) {
        throw EmblemFormatError("USER_DATA007 payload overran checksum boundary");
    }
    if (checksumOffset + 16 > payload.size()) {
        throw EmblemFormatError("USER_DATA007 header.size points beyond the payload boundary");
    }
    const auto expectedChecksum = QByteArray(reinterpret_cast<const char*>(payload.data() + static_cast<std::ptrdiff_t>(checksumOffset)), 16);
    const auto observedChecksum = QCryptographicHash::hash(
        QByteArray(reinterpret_cast<const char*>(payload.data() + 20), static_cast<int>(checksumOffset - 20)),
        QCryptographicHash::Md5);
    if (observedChecksum != expectedChecksum) {
        throw EmblemFormatError("USER_DATA007 checksum mismatch; fail-closed before using emblem data");
    }
    return container;
}

std::vector<std::uint8_t> serializePlaintext(const UserDataContainer& container) {
    if (container.iv.size() != 16) {
        throw EmblemFormatError("USER_DATA007 IV must be 16 bytes");
    }
    std::vector<std::uint8_t> body;
    for (const auto& file : container.files) {
        const auto fileBytes = serializeUserDataFile(file);
        body.insert(body.end(), fileBytes.begin(), fileBytes.end());
    }
    appendPod(body, static_cast<std::int32_t>(container.extraFiles.size()));
    for (const auto& file : container.extraFiles) {
        const auto fileBytes = serializeUserDataFile(file);
        body.insert(body.end(), fileBytes.begin(), fileBytes.end());
    }

    std::vector<std::uint8_t> headerRest;
    appendPod(headerRest, container.unk1);
    appendPod(headerRest, container.unk2);
    appendPod(headerRest, container.unk3);
    appendPod(headerRest, container.unk4);
    appendPod(headerRest, static_cast<std::int32_t>(container.files.size()));

    const auto sizeField = static_cast<std::int32_t>(container.iv.size() + headerRest.size() + body.size());
    const auto checksumPayload = QByteArray(reinterpret_cast<const char*>(headerRest.data()), static_cast<int>(headerRest.size())) +
                                 QByteArray(reinterpret_cast<const char*>(body.data()), static_cast<int>(body.size()));
    const auto checksum = QCryptographicHash::hash(checksumPayload, QCryptographicHash::Md5);

    std::vector<std::uint8_t> plaintext(container.iv.begin(), container.iv.end());
    appendPod(plaintext, sizeField);
    plaintext.insert(plaintext.end(), headerRest.begin(), headerRest.end());
    plaintext.insert(plaintext.end(), body.begin(), body.end());
    plaintext.insert(plaintext.end(), reinterpret_cast<const std::uint8_t*>(checksum.constData()),
                     reinterpret_cast<const std::uint8_t*>(checksum.constData() + checksum.size()));
    while (plaintext.size() % 16 != 0) {
        plaintext.push_back(0);
    }
    return plaintext;
}

EmblemRecord parseEmblemRecord(const std::vector<std::uint8_t>& payload, std::optional<EmblemProvenance> provenance) {
    EmblemRecord record;
    record.provenance = std::move(provenance);
    record.rawBlocks = deserializeBlockContainer(payload);
    bool hasCategory = false;
    bool hasDateTime = false;
    bool hasImage = false;
    for (const auto& block : record.rawBlocks) {
        if (block.name == "Category") {
            if (block.payload.size() != 1) {
                throw EmblemFormatError("Category block must be exactly one byte");
            }
            record.category = static_cast<std::int8_t>(block.payload.front());
            hasCategory = true;
        } else if (block.name == "UgcID") {
            record.ugcId = decodeUtf16LeNullTerminated(block.payload);
        } else if (block.name == "CreatorID") {
            if (block.payload.size() != sizeof(std::int64_t)) {
                throw EmblemFormatError("CreatorID block size mismatch");
            }
            std::int64_t value{};
            std::memcpy(&value, block.payload.data(), sizeof(value));
            record.creatorId = value;
        } else if (block.name == "DateTime") {
            record.dateTime = DateTimeBlock::deserialize(block.payload);
            hasDateTime = true;
        } else if (block.name == "Image") {
            record.image = deserializeImage(block.payload);
            hasImage = true;
        }
    }
    if (!hasCategory || !hasDateTime || !hasImage) {
        throw EmblemFormatError("EMBC payload missing required blocks");
    }
    return record;
}

std::vector<std::uint8_t> serializeEmblemRecord(const EmblemRecord& record) {
    auto blocks = record.rawBlocks;
    if (blocks.empty()) {
        blocks.push_back(EmblemBlock{.name = "Category", .payload = {}});
        blocks.push_back(EmblemBlock{.name = "UgcID", .payload = {}});
        if (record.creatorId.has_value()) {
            blocks.push_back(EmblemBlock{.name = "CreatorID", .payload = {}});
        }
        blocks.push_back(EmblemBlock{.name = "DateTime", .payload = {}});
        blocks.push_back(EmblemBlock{.name = "Image", .payload = {}});
    }

    auto upsertBlock = [&](const std::string& name, std::vector<std::uint8_t> payload) {
        const auto it = std::find_if(blocks.begin(), blocks.end(), [&](const auto& block) {
            return block.name == name;
        });
        if (it != blocks.end()) {
            it->payload = std::move(payload);
            return;
        }
        blocks.push_back(EmblemBlock{.name = name, .payload = std::move(payload)});
    };

    auto eraseBlock = [&](const std::string& name) {
        blocks.erase(
            std::remove_if(blocks.begin(), blocks.end(), [&](const auto& block) {
                return block.name == name;
            }),
            blocks.end());
    };

    upsertBlock("Category", {static_cast<std::uint8_t>(record.category)});
    upsertBlock("UgcID", encodeUtf16LeNullTerminated(record.ugcId));
    if (record.creatorId.has_value()) {
        std::vector<std::uint8_t> creatorBytes;
        appendPod(creatorBytes, *record.creatorId);
        upsertBlock("CreatorID", std::move(creatorBytes));
    } else {
        eraseBlock("CreatorID");
    }
    upsertBlock("DateTime", record.dateTime.serialize());
    upsertBlock("Image", serializeImage(record.image));
    return serializeBlockContainer(blocks);
}

constexpr int kEmblemEntriesPerUserBucket = 19;

bool isLegacyShareBucket(const std::string& bucket) {
    return bucket == "extraFiles" || bucket == "share";
}

bool isUserSlotIndexValid(int slotIndex) {
    return slotIndex >= 0 && slotIndex < 4;
}

int firstFileIndexForUserSlot(const int slotIndex) {
    return slotIndex * kEmblemEntriesPerUserBucket;
}

int localUserEntryIndex(const int fileIndex) {
    return (fileIndex % kEmblemEntriesPerUserBucket) + 1;
}

std::string formatEmblemTargetLabel(const int slotIndex, const int fileIndex) {
    const auto bucket = slotIndex == 0
        ? contracts::SourceBucket::User1
        : slotIndex == 1
            ? contracts::SourceBucket::User2
            : slotIndex == 2
                ? contracts::SourceBucket::User3
                : contracts::SourceBucket::User4;
    return contracts::toDisplayLabel(bucket)
        + " / 徽章 " + QStringLiteral("%1").arg(localUserEntryIndex(fileIndex), 2, 10, QChar('0')).toStdString();
}

contracts::SourceBucket classifyEmblemSourceBucket(const std::string& bucket, int fileIndex) {
    if (isLegacyShareBucket(bucket)) {
        return contracts::SourceBucket::Share;
    }
    const int normalized = std::max(0, fileIndex) / kEmblemEntriesPerUserBucket;
    switch (std::clamp(normalized, 0, 3)) {
    case 0:
        return contracts::SourceBucket::User1;
    case 1:
        return contracts::SourceBucket::User2;
    case 2:
        return contracts::SourceBucket::User3;
    case 3:
    default:
        return contracts::SourceBucket::User4;
    }
}

contracts::WriteCapability emblemWriteCapabilityFor(const std::string& bucket) {
    return bucket == "files" ? contracts::WriteCapability::Editable : contracts::WriteCapability::ReadOnly;
}

bool hasContiguousPrefixThrough(const std::vector<int>& usedSlots, int targetSlotIndex) {
    std::set<int> used(usedSlots.begin(), usedSlots.end());
    for (int slot = 0; slot < targetSlotIndex; ++slot) {
        if (!used.contains(slot)) {
            return false;
        }
    }
    return true;
}

void validateImportTargetSlot(int targetUserSlotIndex) {
    if (!isUserSlotIndexValid(targetUserSlotIndex)) {
        throw EmblemFormatError("Target user slot must be one of user1..user4");
    }
}

int firstAvailableUserSlot(const std::vector<int>& usedSlots) {
    std::set<int> used(usedSlots.begin(), usedSlots.end());
    for (int candidate = 0; candidate < 4; ++candidate) {
        if (!used.contains(candidate)) {
            return candidate;
        }
    }
    return -1;
}

std::optional<int> nextWritableFileIndexForSnapshotImpl(
    const EmblemCatalogSnapshot& snapshot,
    const int targetUserSlotIndex) {
    validateImportTargetSlot(targetUserSlotIndex);
    const int targetFileIndex = firstFileIndexForUserSlot(targetUserSlotIndex) + snapshot.userBucketCounts[static_cast<std::size_t>(targetUserSlotIndex)];
    if (snapshot.userBucketCounts[static_cast<std::size_t>(targetUserSlotIndex)] >= kEmblemEntriesPerUserBucket) {
        return std::nullopt;
    }
    if (snapshot.userFileCount < firstFileIndexForUserSlot(targetUserSlotIndex)) {
        return std::nullopt;
    }
    if (snapshot.userFileCount != targetFileIndex) {
        return std::nullopt;
    }
    return targetFileIndex;
}

int nextWritableFileIndexForContainer(
    const UserDataContainer& container,
    const int targetUserSlotIndex) {
    validateImportTargetSlot(targetUserSlotIndex);
    const int firstIndex = firstFileIndexForUserSlot(targetUserSlotIndex);
    if (static_cast<int>(container.files.size()) < firstIndex) {
        throw EmblemFormatError("Target user slot is sparse and cannot be written safely");
    }

    int nextIndex = firstIndex;
    const int endIndex = firstIndex + kEmblemEntriesPerUserBucket;
    while (nextIndex < endIndex && nextIndex < static_cast<int>(container.files.size())) {
        if (container.files[static_cast<std::size_t>(nextIndex)].fileType != "EMBC") {
            throw EmblemFormatError("Target user slot is not writable in current save layout");
        }
        ++nextIndex;
    }
    if (nextIndex >= endIndex) {
        throw EmblemFormatError("Target user slot is already full");
    }
    if (nextIndex != static_cast<int>(container.files.size())) {
        throw EmblemFormatError("Target user slot is sparse and cannot be written safely");
    }
    return nextIndex;
}

std::vector<contracts::ImportTargetChoiceDto> buildTargetChoicesImpl(const EmblemCatalogSnapshot& snapshot) {
    std::vector<contracts::ImportTargetChoiceDto> choices;
    choices.reserve(4U);
    for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
        const auto nextFileIndex = nextWritableFileIndexForSnapshotImpl(snapshot, slotIndex);
        if (!nextFileIndex.has_value()) {
            continue;
        }
        choices.push_back({
            .code = slotIndex,
            .label = contracts::toDisplayLabel(classifyEmblemSourceBucket("files", firstFileIndexForUserSlot(slotIndex))),
            .note = "下一写入栏位：" + formatEmblemTargetLabel(slotIndex, *nextFileIndex),
        });
    }
    return choices;
}

std::optional<int> findSuggestedUserBucket(const EmblemCatalogSnapshot& snapshot) {
    for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
        if (nextWritableFileIndexForSnapshotImpl(snapshot, slotIndex).has_value()) {
            return slotIndex;
        }
    }
    return std::nullopt;
}

std::pair<std::string, int> parseAssetId(const std::string& assetId) {
    const auto delimiter = assetId.find(':');
    if (delimiter == std::string::npos) {
        throw EmblemFormatError("Invalid asset id: " + assetId);
    }
    return {assetId.substr(0, delimiter), std::stoi(assetId.substr(delimiter + 1))};
}

}  // namespace

std::optional<int> nextWritableFileIndexForSnapshot(
    const EmblemCatalogSnapshot& snapshot,
    const int targetUserSlotIndex) {
    return nextWritableFileIndexForSnapshotImpl(snapshot, targetUserSlotIndex);
}

std::vector<contracts::ImportTargetChoiceDto> buildTargetChoices(const EmblemCatalogSnapshot& snapshot) {
    return buildTargetChoicesImpl(snapshot);
}

DateTimeBlock DateTimeBlock::now() {
    const auto now = QDateTime::currentDateTimeUtc();
    return DateTimeBlock{
        .filetime = dateTimeToFiletime(now),
        .packedSystemTime = packSystemTime(now),
    };
}

DateTimeBlock DateTimeBlock::deserialize(const std::vector<std::uint8_t>& payload) {
    if (payload.size() != 16) {
        throw EmblemFormatError("DateTime block must be 16 bytes");
    }
    DateTimeBlock block;
    std::memcpy(&block.filetime, payload.data(), sizeof(block.filetime));
    std::memcpy(&block.packedSystemTime, payload.data() + 8, sizeof(block.packedSystemTime));
    return block;
}

std::vector<std::uint8_t> DateTimeBlock::serialize() const {
    std::vector<std::uint8_t> buffer;
    appendPod(buffer, filetime);
    appendPod(buffer, packedSystemTime);
    return buffer;
}

std::string DateTimeBlock::toIso8601() const {
    return filetimeToDateTime(filetime).toString(Qt::ISODate).toStdString();
}

UserDataContainer UserData007Codec::readEncrypted(const std::filesystem::path& path) const {
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly)) {
        throw EmblemFormatError("Failed to open USER_DATA007: " + path.generic_string());
    }
    const QByteArray raw = file.readAll();
    if (raw.size() < 16 || ((raw.size() - 16) % 16) != 0) {
        throw EmblemFormatError("Encrypted USER_DATA007 length is invalid");
    }

    std::array<std::uint8_t, 16> iv{};
    std::memcpy(iv.data(), raw.constData(), iv.size());
    const std::vector<std::uint8_t> ciphertext(
        reinterpret_cast<const std::uint8_t*>(raw.constData() + 16),
        reinterpret_cast<const std::uint8_t*>(raw.constData() + raw.size()));
    std::vector<std::uint8_t> plaintext(iv.begin(), iv.end());
    const auto tail = decryptAesCbc(ciphertext, iv);
    plaintext.insert(plaintext.end(), tail.begin(), tail.end());
    return deserializePlaintext(plaintext);
}

std::vector<std::uint8_t> UserData007Codec::writeEncrypted(const UserDataContainer& container) const {
    const auto plaintext = serializePlaintext(container);
    std::vector<std::uint8_t> output(container.iv.begin(), container.iv.end());
    const std::vector<std::uint8_t> plaintextTail(plaintext.begin() + 16, plaintext.end());
    const auto ciphertext = encryptAesCbc(plaintextTail, container.iv);
    output.insert(output.end(), ciphertext.begin(), ciphertext.end());
    return output;
}

EmblemCatalogSnapshot buildCatalogSnapshot(const UserDataContainer& container) {
    EmblemCatalogSnapshot snapshot;
    snapshot.userFileCount = static_cast<int>(container.files.size());

    auto appendSelection = [&](const std::string& bucket, const std::vector<UserDataFile>& files) {
        for (int index = 0; index < static_cast<int>(files.size()); ++index) {
            const auto& file = files[index];
            if (file.fileType != "EMBC") {
                continue;
            }
            EmblemSelection selection;
            selection.assetId = bucket + ":" + std::to_string(index);
            selection.sourceBucket = bucket;
            selection.fileIndex = index;
            selection.rawPayload = file.data;
            selection.record = parseEmblemRecord(
                file.data,
                EmblemProvenance{
                    .sourceBucket = bucket,
                    .fileIndex = index,
                    .fileType = file.fileType,
                });
            snapshot.selections.push_back(selection);

            const auto sourceBucket = classifyEmblemSourceBucket(bucket, index);
            const auto previewResolution = preview::makeUnknownPreview(
                "preview-disabled.temporarily",
                std::make_optional<std::string>("当前分支已临时关闭徽章预览功能。"),
                std::nullopt);

            contracts::CatalogItemDto dto;
            dto.assetId = selection.assetId;
            dto.assetKind = contracts::AssetKind::Emblem;
            dto.sourceBucket = sourceBucket;
            dto.slotIndex = contracts::slotIndexOf(sourceBucket);
            dto.displayName = selection.record.ugcId.empty()
                ? "无名称 / 无分享码"
                : selection.record.ugcId;
            dto.shareCode = selection.record.ugcId;
            dto.previewState = contracts::PreviewState::Unknown;
            dto.writeCapability = emblemWriteCapabilityFor(bucket);
            dto.recordRef = "USER_DATA007/" + bucket + "/" + std::to_string(index);
            dto.detailProvenance = bucket == "files"
                ? "emblem-user-bucket-19-group-v1"
                : "emblem-share-extraFiles-v1";
            dto.slotLabel = contracts::formatUserSlotLabel(bucket == "files" ? std::optional<int>{index} : std::nullopt);
            dto.origin = bucket == "files" ? contracts::AssetOrigin::User : contracts::AssetOrigin::Share;
            dto.editable = dto.writeCapability == contracts::WriteCapability::Editable;
            dto.sourceLabel = contracts::toDisplayLabel(sourceBucket);
            dto.description = bucket == "files" ? "使用者栏位徽章" : "分享栏位徽章";
            dto.tags = bucket == "files"
                ? std::vector<std::string>{"user", "stable-emblem", contracts::toString(sourceBucket)}
                : std::vector<std::string>{"share", "stable-emblem"};
            dto.detailFields.push_back({"Name", "无（游戏内徽章没有独立名称字段）"});
            dto.detailFields.push_back({"ShareCode", selection.record.ugcId.empty() ? "-" : selection.record.ugcId});
            dto.detailFields.push_back({"Category", std::to_string(selection.record.category)});
            dto.detailFields.push_back({"AssetKind", contracts::toString(dto.assetKind)});
            dto.detailFields.push_back({"SourceBucket", contracts::toString(dto.sourceBucket)});
            dto.detailFields.push_back({"WriteCapability", contracts::toString(dto.writeCapability)});
            dto.detailFields.push_back({"RecordRef", dto.recordRef});
            dto.detailFields.push_back({"DetailProvenance", dto.detailProvenance});
            dto.detailFields.push_back({"BlockCount", std::to_string(selection.record.rawBlocks.size())});
            if (selection.record.creatorId.has_value()) {
                dto.detailFields.push_back({"CreatorID", std::to_string(*selection.record.creatorId)});
            }
            dto.detailFields.push_back({"DateTime", selection.record.dateTime.toIso8601()});
            dto.preview = previewResolution.handle;
            dto.preview.provenance = previewResolution.provenanceTag;
            snapshot.catalog.push_back(dto);

            if (bucket == "files" && dto.slotIndex.has_value()) {
                snapshot.userSlotsInUse.push_back(*dto.slotIndex);
                snapshot.userBucketCounts[static_cast<std::size_t>(*dto.slotIndex)] += 1;
            }
        }
    };

    appendSelection("files", container.files);
    appendSelection("extraFiles", container.extraFiles);
    std::sort(snapshot.userSlotsInUse.begin(), snapshot.userSlotsInUse.end());
    snapshot.userSlotsInUse.erase(std::unique(snapshot.userSlotsInUse.begin(), snapshot.userSlotsInUse.end()), snapshot.userSlotsInUse.end());
    return snapshot;
}

std::vector<std::uint8_t> serializeEmblemRecordForTests(const EmblemRecord& record) {
    return serializeEmblemRecord(record);
}

const EmblemSelection& findSelectionByAssetId(const EmblemCatalogSnapshot& snapshot, const std::string& assetId) {
    const auto it = std::find_if(snapshot.selections.begin(), snapshot.selections.end(), [&](const auto& selection) {
        return selection.assetId == assetId;
    });
    if (it == snapshot.selections.end()) {
        throw EmblemFormatError("Selected emblem asset was not found");
    }
    return *it;
}

std::vector<std::uint8_t> exportSelectionPayload(const EmblemCatalogSnapshot& snapshot, const std::string& assetId) {
    return findSelectionByAssetId(snapshot, assetId).rawPayload;
}

std::vector<std::uint8_t> buildEditableClonePayload(
    const std::vector<std::uint8_t>& rawPayload,
    std::uint8_t editableCategory) {
    auto blocks = deserializeBlockContainer(rawPayload);
    bool patched = false;
    for (auto& block : blocks) {
        if (block.name != "Category") {
            continue;
        }
        if (block.payload.size() != 1U) {
            throw EmblemFormatError("Category block must be exactly one byte");
        }
        block.payload.front() = editableCategory;
        patched = true;
        break;
    }
    if (!patched) {
        throw EmblemFormatError("EMBC payload missing Category block");
    }
    return serializeBlockContainer(blocks);
}

UserDataContainer applyEmblemPayloadToUserSlot(
    const UserDataContainer& container,
    const std::vector<std::uint8_t>& rawPayload,
    int targetUserSlotIndex,
    int* assignedSlot) {
    validateImportTargetSlot(targetUserSlotIndex);
    UserDataFile importedFile;
    importedFile.fileType = "EMBC";
    importedFile.data = buildEditableClonePayload(rawPayload, kEmblemImportedUserCategory);

    UserDataContainer updated = container;
    const int targetFileIndex = nextWritableFileIndexForContainer(updated, targetUserSlotIndex);
    updated.files.push_back(importedFile);

    if (assignedSlot != nullptr) {
        *assignedSlot = targetFileIndex;
    }
    return updated;
}

contracts::ImportPlanDto buildSingleShareImportPlan(
    const EmblemCatalogSnapshot& snapshot,
    const std::string& assetId,
    std::optional<int> targetUserSlotIndex) {
    contracts::ImportPlanDto plan;
    plan.title = "导入计划";
    plan.sourceAssetIds = {assetId};
    plan.mode = "in-place-update";
    plan.summary = "将所选分享徽章导入指定使用者栏位，并直接更新当前已打开存档。";
    plan.dryRunRequired = true;
    plan.shadowWorkspaceRequired = true;
    plan.postWriteReadbackRequired = true;
    plan.targetChoices = buildTargetChoices(snapshot);

    const auto it = std::find_if(snapshot.selections.begin(), snapshot.selections.end(), [&](const auto& selection) {
        return selection.assetId == assetId;
    });
    if (it == snapshot.selections.end()) {
        plan.blockers.push_back("所选分享徽章不存在");
        plan.targetSlot = "无";
        return plan;
    }
    if (!isLegacyShareBucket(it->sourceBucket)) {
        plan.blockers.push_back("当前仅支持从分享栏位导入");
        plan.targetSlot = "无";
        return plan;
    }
    if (!targetUserSlotIndex.has_value()) {
        const int suggestedTargetSlot = findSuggestedUserBucket(snapshot).value_or(0);
        plan.targetSlot = "待用户选择";
        plan.suggestedTargetUserSlotIndex = suggestedTargetSlot;
        plan.warnings.push_back("将在下一步由用户显式选择目标使用者栏位；写入时会顺序追加到该使用者的下一个徽章位置。");
        plan.operations.push_back({
            "推荐目标使用者栏位",
            contracts::toDisplayLabel(classifyEmblemSourceBucket("files", firstFileIndexForUserSlot(suggestedTargetSlot))),
            "suggested",
            ""
        });
        if (plan.targetChoices.empty()) {
            plan.blockers.push_back("当前没有可安全写入的使用者徽章栏位。");
            plan.summary = "导入计划被阻断。";
        }
        return plan;
    }
    if (!isUserSlotIndexValid(*targetUserSlotIndex)) {
        plan.blockers.push_back("目标使用者栏位必须为 使用者1/2/3/4");
        plan.targetSlot = "无效栏位";
        return plan;
    }

    const int targetSlot = *targetUserSlotIndex;
    const auto targetFileIndex = nextWritableFileIndexForSnapshot(snapshot, targetSlot);
    if (!targetFileIndex.has_value()) {
        plan.blockers.push_back("目标使用者栏位当前不可安全追加，或该栏位已经写满。");
        plan.targetSlot = "无效栏位";
        return plan;
    }
    plan.targetSlot = formatEmblemTargetLabel(targetSlot, *targetFileIndex);
    plan.suggestedTargetUserSlotIndex = targetSlot;
    plan.operations.push_back({"选择分享徽章", assetId, "selected", ""});
    plan.operations.push_back({"目标使用者栏位", plan.targetSlot, "append", "所选分享徽章将顺序追加到该使用者栏位的下一个位置。"});
    plan.operations.push_back({"保存方式", "当前已打开存档", "in-place-update", ""});
    plan.warnings.push_back("写入前在 staging workspace 中执行，写后必须 readback。");
    return plan;
}

UserDataContainer applySingleShareImport(
    const UserDataContainer& container,
    const std::string& assetId,
    int targetUserSlotIndex,
    int* assignedSlot) {
    validateImportTargetSlot(targetUserSlotIndex);
    const auto [bucket, index] = parseAssetId(assetId);
    if (!isLegacyShareBucket(bucket)) {
        throw EmblemFormatError("Only share emblem import is supported");
    }
    if (index < 0 || index >= static_cast<int>(container.extraFiles.size())) {
        throw EmblemFormatError("Share emblem index out of range");
    }

    const auto& shareFile = container.extraFiles[static_cast<std::size_t>(index)];
    if (shareFile.fileType != "EMBC") {
        throw EmblemFormatError("Selected share entry is not EMBC");
    }

    return applyEmblemPayloadToUserSlot(container, shareFile.data, targetUserSlotIndex, assignedSlot);
}

UserDataContainer applySingleShareImport(
    const UserDataContainer& container,
    const std::string& assetId,
    int* assignedSlot) {
    const auto snapshot = buildCatalogSnapshot(container);
    const int nextSlot = findSuggestedUserBucket(snapshot).value_or(-1);
    if (nextSlot < 0) {
        throw EmblemFormatError("No empty user slot available");
    }
    return applySingleShareImport(container, assetId, nextSlot, assignedSlot);
}

EmblemImportReadback verifySingleShareImportReadback(
    const EmblemCatalogSnapshot& snapshot,
    int targetUserSlotIndex,
    const std::vector<std::uint8_t>& expectedPayload) {
    validateImportTargetSlot(targetUserSlotIndex);
    const int targetFileIndex = firstFileIndexForUserSlot(targetUserSlotIndex)
        + snapshot.userBucketCounts[static_cast<std::size_t>(targetUserSlotIndex)] - 1;
    const auto selectionIt = std::find_if(snapshot.selections.begin(), snapshot.selections.end(), [&](const auto& selection) {
        return selection.sourceBucket == "files" && selection.fileIndex == targetFileIndex;
    });
    if (selectionIt == snapshot.selections.end()) {
        throw EmblemFormatError("Readback failed: target user slot is missing after import");
    }
    if (selectionIt->rawPayload != expectedPayload) {
        throw EmblemFormatError("Readback failed: target user slot payload mismatch");
    }
    return EmblemImportReadback{
        .targetUserSlotIndex = targetUserSlotIndex,
        .targetFileIndex = targetFileIndex,
        .selection = *selectionIt,
    };
}

}  // namespace ac6dm::emblem
