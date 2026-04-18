#include "ac_runtime_regulation.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <array>
#include <fstream>
#include <iterator>
#include <memory>

namespace ac6dm::ac {
namespace {

constexpr std::array<unsigned char, 16> kAcDataKey = {
    0xB1, 0x56, 0x87, 0x9F, 0x13, 0x48, 0x97, 0x98,
    0x70, 0x05, 0xC4, 0x87, 0x00, 0xAE, 0xF8, 0x79
};
constexpr std::size_t kIvBytes = 16U;
constexpr std::size_t kRegulationHeaderBytes = 20U;

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::uint32_t readLe32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    if (offset + sizeof(std::uint32_t) > bytes.size()) {
        return 0;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + static_cast<std::ptrdiff_t>(offset), sizeof(value));
    return value;
}

std::string sha256Hex(const std::vector<std::uint8_t>& bytes) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(bytes.data(), bytes.size(), digest.data());
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string output;
    output.reserve(digest.size() * 2U);
    for (const auto value : digest) {
        output.push_back(kHexDigits[(value >> 4U) & 0x0F]);
        output.push_back(kHexDigits[value & 0x0F]);
    }
    return output;
}

std::vector<std::uint8_t> decryptAesCbcNoPadding(const std::vector<std::uint8_t>& encryptedBytes) {
    if (encryptedBytes.size() <= kIvBytes || ((encryptedBytes.size() - kIvBytes) % 16U) != 0U) {
        return {};
    }

    const auto* iv = encryptedBytes.data();
    const auto* ciphertext = encryptedBytes.data() + static_cast<std::ptrdiff_t>(kIvBytes);
    const int ciphertextBytes = static_cast<int>(encryptedBytes.size() - kIvBytes);

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context) {
        return {};
    }
    if (EVP_DecryptInit_ex(context.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv) != 1) {
        return {};
    }
    if (EVP_CIPHER_CTX_set_padding(context.get(), 0) != 1) {
        return {};
    }

    std::vector<std::uint8_t> plaintext(static_cast<std::size_t>(ciphertextBytes), 0);
    int outputBytes = 0;
    int totalBytes = 0;
    if (EVP_DecryptUpdate(
            context.get(),
            plaintext.data(),
            &outputBytes,
            ciphertext,
            ciphertextBytes) != 1) {
        return {};
    }
    totalBytes += outputBytes;
    if (EVP_DecryptFinal_ex(context.get(), plaintext.data() + totalBytes, &outputBytes) != 1) {
        return {};
    }
    totalBytes += outputBytes;
    plaintext.resize(static_cast<std::size_t>(totalBytes));
    return plaintext;
}

std::filesystem::path findUserData010(const std::filesystem::path& unpackedSaveDir) {
    const auto direct = unpackedSaveDir / "USER_DATA010";
    if (std::filesystem::exists(direct)) {
        return direct;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(unpackedSaveDir)) {
        if (entry.is_regular_file() && entry.path().filename() == "USER_DATA010") {
            return entry.path();
        }
    }
    return {};
}

}  // namespace

RuntimeRegulationSnapshotInfo inspectRegulationSnapshotPlaintext(
    const std::vector<std::uint8_t>& plaintext,
    std::filesystem::path sourceFile) {
    RuntimeRegulationSnapshotInfo info;
    info.sourceFile = std::move(sourceFile);
    info.filePresent = true;
    info.decrypted = !plaintext.empty();
    info.plaintextBytes = plaintext.size();

    if (plaintext.empty()) {
        info.note = "USER_DATA010 plaintext is empty.";
        return info;
    }

    info.plaintextSha256 = sha256Hex(plaintext);
    if (plaintext.size() < kRegulationHeaderBytes) {
        info.note = "USER_DATA010 plaintext is smaller than the expected 20-byte regulation header.";
        return info;
    }

    info.containerSizeField = readLe32(plaintext, 0);
    info.containerMagic.assign(
        reinterpret_cast<const char*>(plaintext.data() + 4),
        reinterpret_cast<const char*>(plaintext.data() + 8));
    info.versionField = readLe32(plaintext, 8);
    info.buildField = readLe32(plaintext, 12);
    info.embeddedOffset = readLe32(plaintext, 16) == 0 ? static_cast<std::uint32_t>(kRegulationHeaderBytes)
                                                        : static_cast<std::uint32_t>(kRegulationHeaderBytes);

    const auto& metadata = regulationPresetCatalog().runtimeMetadata;
    info.headerQualified =
        info.containerMagic == metadata.validatedUserData010HeaderMagic
        && info.versionField == metadata.validatedUserData010Version
        && plaintext.size() >= metadata.validatedUserData010EmbeddedOffset;

    info.matchesValidatedSnapshot =
        info.headerQualified
        && metadata.validatedUserData010PlainLength == plaintext.size()
        && metadata.validatedUserData010PlainSha256 == info.plaintextSha256;

    if (info.matchesValidatedSnapshot) {
        info.note = "Runtime USER_DATA010 plaintext matches the validated regulation snapshot bound to the vendored preset catalog.";
    } else if (info.headerQualified) {
        info.note = "Runtime USER_DATA010 header matches the expected regulation container shape, but the plaintext SHA-256 differs from the vendored validated snapshot.";
    } else {
        info.note = "Runtime USER_DATA010 plaintext does not match the expected regulation container header.";
    }

    return info;
}

RuntimeRegulationSnapshotInfo inspectRuntimeRegulationSnapshot(const std::filesystem::path& unpackedSaveDir) {
    RuntimeRegulationSnapshotInfo info;
    const auto sourceFile = findUserData010(unpackedSaveDir);
    if (sourceFile.empty()) {
        info.note = "USER_DATA010 was not found in the unpacked save directory.";
        return info;
    }

    info.sourceFile = sourceFile;
    info.filePresent = true;
    const auto encryptedBytes = readBytes(sourceFile);
    if (encryptedBytes.empty()) {
        info.note = "USER_DATA010 could not be read.";
        return info;
    }

    const auto plaintext = decryptAesCbcNoPadding(encryptedBytes);
    if (plaintext.empty()) {
        info.note = "USER_DATA010 AES-CBC decryption failed.";
        return info;
    }

    auto inspected = inspectRegulationSnapshotPlaintext(plaintext, sourceFile);
    inspected.filePresent = true;
    inspected.decrypted = true;
    return inspected;
}

const RegulationPresetCatalog* resolveRuntimeRegulationCatalog(
    const std::filesystem::path& unpackedSaveDir,
    RuntimeRegulationSnapshotInfo* outInfo) {
    const auto info = inspectRuntimeRegulationSnapshot(unpackedSaveDir);
    if (outInfo != nullptr) {
        *outInfo = info;
    }
    if (!info.matchesValidatedSnapshot) {
        return nullptr;
    }
    return &regulationPresetCatalog();
}

}  // namespace ac6dm::ac
