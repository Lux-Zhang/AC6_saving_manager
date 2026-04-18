#include "../core/ac/ac_runtime_regulation.hpp"

#include <gtest/gtest.h>

#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace {

constexpr std::array<unsigned char, 16> kAcDataKey = {
    0xB1, 0x56, 0x87, 0x9F, 0x13, 0x48, 0x97, 0x98,
    0x70, 0x05, 0xC4, 0x87, 0x00, 0xAE, 0xF8, 0x79
};

std::vector<std::uint8_t> makePlaintextFixture() {
    std::vector<std::uint8_t> bytes(64U, 0);
    const std::uint32_t field0 = 0x00000030U;
    const std::uint32_t version = 2U;
    const std::uint32_t build = 0x00A6795AU;
    const std::uint32_t embeddedField = 0x00000020U;
    std::memcpy(bytes.data() + 0, &field0, sizeof(field0));
    bytes[4] = 0x20;
    bytes[5] = 'G';
    bytes[6] = 'E';
    bytes[7] = 'R';
    std::memcpy(bytes.data() + 8, &version, sizeof(version));
    std::memcpy(bytes.data() + 12, &build, sizeof(build));
    std::memcpy(bytes.data() + 16, &embeddedField, sizeof(embeddedField));
    for (std::size_t index = 20; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(index & 0xFFU);
    }
    return bytes;
}

std::vector<std::uint8_t> encryptFixture(const std::vector<std::uint8_t>& plaintext) {
    const std::array<unsigned char, 16> iv = {
        0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,
        0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED, 0xFE, 0x0F
    };
    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context) {
        throw std::runtime_error("Failed to create OpenSSL cipher context");
    }
    if (EVP_EncryptInit_ex(context.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv.data()) != 1) {
        throw std::runtime_error("Failed to initialize fixture AES context");
    }
    if (EVP_CIPHER_CTX_set_padding(context.get(), 0) != 1) {
        throw std::runtime_error("Failed to disable padding for fixture AES context");
    }

    std::vector<std::uint8_t> ciphertext(plaintext.size(), 0);
    int outBytes = 0;
    int totalBytes = 0;
    if (EVP_EncryptUpdate(
            context.get(),
            ciphertext.data(),
            &outBytes,
            plaintext.data(),
            static_cast<int>(plaintext.size())) != 1) {
        throw std::runtime_error("Failed during fixture AES encryption");
    }
    totalBytes += outBytes;
    if (EVP_EncryptFinal_ex(context.get(), ciphertext.data() + totalBytes, &outBytes) != 1) {
        throw std::runtime_error("Failed to finalize fixture AES encryption");
    }
    totalBytes += outBytes;
    ciphertext.resize(static_cast<std::size_t>(totalBytes));

    std::vector<std::uint8_t> encrypted(iv.begin(), iv.end());
    encrypted.insert(encrypted.end(), ciphertext.begin(), ciphertext.end());
    return encrypted;
}

TEST(AcRuntimeRegulationTest, InspectPlaintextParsesExpectedHeader) {
    const auto plaintext = makePlaintextFixture();
    const auto info = ac6dm::ac::inspectRegulationSnapshotPlaintext(plaintext);

    EXPECT_TRUE(info.filePresent);
    EXPECT_TRUE(info.decrypted);
    EXPECT_TRUE(info.headerQualified);
    EXPECT_EQ(info.containerMagic, " GER");
    EXPECT_EQ(info.versionField, 2U);
    EXPECT_EQ(info.embeddedOffset, 20U);
    EXPECT_FALSE(info.plaintextSha256.empty());
    EXPECT_FALSE(info.matchesValidatedSnapshot);
}

TEST(AcRuntimeRegulationTest, InspectRuntimeSnapshotDecryptsUserData010File) {
    const auto tempRoot = std::filesystem::temp_directory_path() / "ac6dm-runtime-regulation-test";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);

    const auto plaintext = makePlaintextFixture();
    const auto encrypted = encryptFixture(plaintext);
    {
        std::ofstream output(tempRoot / "USER_DATA010", std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
    }

    const auto info = ac6dm::ac::inspectRuntimeRegulationSnapshot(tempRoot);

    EXPECT_TRUE(info.filePresent);
    EXPECT_TRUE(info.decrypted);
    EXPECT_TRUE(info.headerQualified);
    EXPECT_EQ(info.sourceFile.filename(), std::filesystem::path("USER_DATA010"));
    EXPECT_FALSE(info.matchesValidatedSnapshot);
    EXPECT_NE(info.note.find("header matches"), std::string::npos);

    std::filesystem::remove_all(tempRoot);
}

TEST(AcRuntimeRegulationTest, ResolveRuntimeCatalogFailsClosedWhenSnapshotDoesNotMatchValidatedHash) {
    const auto tempRoot = std::filesystem::temp_directory_path() / "ac6dm-runtime-regulation-mismatch";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);

    const auto plaintext = makePlaintextFixture();
    const auto encrypted = encryptFixture(plaintext);
    {
        std::ofstream output(tempRoot / "USER_DATA010", std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
    }

    ac6dm::ac::RuntimeRegulationSnapshotInfo info;
    const auto* catalog = ac6dm::ac::resolveRuntimeRegulationCatalog(tempRoot, &info);

    EXPECT_EQ(catalog, nullptr);
    EXPECT_TRUE(info.filePresent);
    EXPECT_TRUE(info.headerQualified);
    EXPECT_FALSE(info.matchesValidatedSnapshot);

    std::filesystem::remove_all(tempRoot);
}

}  // namespace
