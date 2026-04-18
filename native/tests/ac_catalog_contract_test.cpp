#include "core/ac/ac_catalog.hpp"

#include <openssl/evp.h>
#include <gtest/gtest.h>
#include <zlib.h>

#include <algorithm>
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
constexpr std::size_t kPlainSize = 65536;
constexpr std::size_t kHeaderBytes = 20;
constexpr std::size_t kFooterBytes = 28;
constexpr int kMaxRecordsPerUserBucket = 40;

void appendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(value));
}

void appendU64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(value));
}

std::vector<std::uint8_t> fixedCString(const std::string& value) {
    std::vector<std::uint8_t> bytes(16, 0);
    std::copy(value.begin(), value.end(), bytes.begin());
    return bytes;
}

void appendBlock(
    std::vector<std::uint8_t>& bytes,
    const std::string& name,
    const std::vector<std::uint8_t>& payload) {
    const auto nameBytes = fixedCString(name);
    bytes.insert(bytes.end(), nameBytes.begin(), nameBytes.end());
    appendU32(bytes, static_cast<std::uint32_t>(payload.size()));
    appendU32(bytes, 0);
    appendU64(bytes, 0);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

void appendBlockWithControl(
    std::vector<std::uint8_t>& bytes,
    const std::string& name,
    const std::vector<std::uint8_t>& payload,
    const std::uint32_t unk) {
    const auto nameBytes = fixedCString(name);
    bytes.insert(bytes.end(), nameBytes.begin(), nameBytes.end());
    appendU32(bytes, static_cast<std::uint32_t>(payload.size()));
    appendU32(bytes, unk);
    appendU64(bytes, 0);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

void appendDelimiter(std::vector<std::uint8_t>& bytes, const std::string& name) {
    const auto nameBytes = fixedCString(name);
    bytes.insert(bytes.end(), nameBytes.begin(), nameBytes.end());
    appendU32(bytes, 0);
    appendU32(bytes, 0);
    appendU64(bytes, 0);
}

std::vector<std::uint8_t> utf16LeNullTerminated(const std::string& value) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(value.size() * 2U + 2U);
    for (const auto ch : value) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
        bytes.push_back(0);
    }
    bytes.push_back(0);
    bytes.push_back(0);
    return bytes;
}

void writeUtf16LeAt(
    std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::string& value) {
    const auto encoded = utf16LeNullTerminated(value);
    ASSERT_LE(offset + encoded.size(), bytes.size());
    std::copy(encoded.begin(), encoded.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> makeAsmcWrappedPayload(const std::vector<std::uint8_t>& inflatedRecord) {
    uLongf compressedBound = compressBound(static_cast<uLong>(inflatedRecord.size()));
    std::vector<std::uint8_t> compressed(static_cast<std::size_t>(compressedBound), 0);
    auto compressedSize = compressedBound;
    const auto result = ::compress2(
        compressed.data(),
        &compressedSize,
        inflatedRecord.data(),
        static_cast<uLong>(inflatedRecord.size()),
        Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        ADD_FAILURE() << "compress2 failed with code " << result;
        return {};
    }
    compressed.resize(static_cast<std::size_t>(compressedSize));

    std::vector<std::uint8_t> payload;
    payload.insert(payload.end(), {'A', 'S', 'M', 'C'});
    appendU32(payload, 0x00291222U);
    appendU32(payload, static_cast<std::uint32_t>(compressed.size()));
    appendU32(payload, static_cast<std::uint32_t>(inflatedRecord.size()));
    payload.insert(payload.end(), compressed.begin(), compressed.end());
    return payload;
}

std::vector<std::uint8_t> makeRecordPayload(const std::uint8_t seed, const int ordinal) {
    std::vector<std::uint8_t> bytes;
    appendDelimiter(bytes, "---- begin ----");
    appendBlock(bytes, "Category", {static_cast<std::uint8_t>(seed + ordinal)});
    appendBlock(bytes, "DataName", utf16LeNullTerminated("ARCHIVE-" + std::to_string(seed) + "-" + std::to_string(ordinal)));
    appendBlock(bytes, "AcName", utf16LeNullTerminated("FRAME-" + std::to_string(seed) + "-" + std::to_string(ordinal)));
    appendBlock(bytes, "UgcID", utf16LeNullTerminated("CODE-" + std::to_string(seed) + "-" + std::to_string(ordinal)));

    std::vector<std::uint8_t> dateTime(16, 0);
    for (std::size_t index = 0; index < dateTime.size(); ++index) {
        dateTime[index] = static_cast<std::uint8_t>(seed + ordinal + index);
    }
    appendBlock(bytes, "DateTime", dateTime);

    std::vector<std::uint8_t> design(96, static_cast<std::uint8_t>(0x7A + ordinal));
    design[0] = 'A';
    design[1] = 'S';
    design[2] = 'M';
    design[3] = 'C';
    design.resize(0x140, 0);
    writeUtf16LeAt(design, 0x40, "B1YA3PAJGNTL");
    writeUtf16LeAt(design, 0xA2, "ARCHIVE-" + std::to_string(seed) + "-" + std::to_string(ordinal));
    writeUtf16LeAt(design, 0xE2, "FRAME-" + std::to_string(seed) + "-" + std::to_string(ordinal));
    appendBlock(bytes, "Design", design);
    appendDelimiter(bytes, "----  end  ----");
    return bytes;
}

std::vector<std::uint8_t> makeAsmcNamedRecordPayload(
    const std::string& archiveName,
    const std::string& machineName,
    const std::string& shareCode) {
    std::vector<std::uint8_t> innerRecord;
    appendDelimiter(innerRecord, "---- begin ----");
    appendBlock(innerRecord, "UgcID", utf16LeNullTerminated(shareCode));
    appendBlock(innerRecord, "DataName", utf16LeNullTerminated(archiveName));
    appendBlock(innerRecord, "AcName", utf16LeNullTerminated(machineName));
    appendBlockWithControl(innerRecord, "Assemble", std::vector<std::uint8_t>(64, 0x11), 3);
    appendBlockWithControl(innerRecord, "Coloring", std::vector<std::uint8_t>(128, 0x22), 3);
    appendBlockWithControl(innerRecord, "Decal", std::vector<std::uint8_t>(32, 0x33), 1);
    appendBlockWithControl(innerRecord, "Marking", std::vector<std::uint8_t>(16, 0x44), 2);
    appendDelimiter(innerRecord, "----  end  ----");

    std::vector<std::uint8_t> outerRecord;
    appendDelimiter(outerRecord, "---- begin ----");
    appendBlock(outerRecord, "Category", {0x01});
    appendBlock(outerRecord, "DateTime", std::vector<std::uint8_t>(16, 0x22));
    appendBlock(outerRecord, "Design", makeAsmcWrappedPayload(innerRecord));
    appendDelimiter(outerRecord, "----  end  ----");
    return outerRecord;
}

std::vector<std::uint8_t> makeGarbageNamedRecordPayload(const std::uint8_t seed, const int ordinal) {
    std::vector<std::uint8_t> bytes;
    appendDelimiter(bytes, "---- begin ----");
    appendBlock(bytes, "Category", {static_cast<std::uint8_t>(seed + ordinal)});

    std::vector<std::uint8_t> dateTime(16, static_cast<std::uint8_t>(seed + ordinal));
    appendBlock(bytes, "DateTime", dateTime);

    std::vector<std::uint8_t> design(0x140, static_cast<std::uint8_t>(0x7A + ordinal));
    design[0] = 'A';
    design[1] = 'S';
    design[2] = 'M';
    design[3] = 'C';
    for (std::size_t index = 0x40; index < 0x100 && index < design.size(); ++index) {
        design[index] = static_cast<std::uint8_t>(0x80 + ((index + ordinal) % 0x40));
    }
    appendBlock(bytes, "Design", design);
    appendDelimiter(bytes, "----  end  ----");
    return bytes;
}

std::uint32_t firstMagicOffset(const std::vector<std::uint8_t>& record) {
    for (std::size_t offset = 0; offset + 4 <= record.size(); ++offset) {
        if (record[offset] == 'A' && record[offset + 1] == 'S' && record[offset + 2] == 'M' && record[offset + 3] == 'C') {
            return static_cast<std::uint32_t>(offset);
        }
    }
    return 0;
}

std::vector<std::uint8_t> encryptPlaintext(
    const std::vector<std::uint8_t>& plaintext,
    const std::array<std::uint8_t, 16>& iv) {
    EVP_CIPHER_CTX* rawCtx = EVP_CIPHER_CTX_new();
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(rawCtx, &EVP_CIPHER_CTX_free);
    EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv.data());
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
    std::vector<std::uint8_t> output(plaintext.size() + 16, 0);
    int outLen1 = 0;
    EVP_EncryptUpdate(ctx.get(), output.data(), &outLen1, plaintext.data(), static_cast<int>(plaintext.size()));
    int outLen2 = 0;
    EVP_EncryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2);
    output.resize(static_cast<std::size_t>(outLen1 + outLen2));
    return output;
}

std::vector<std::uint8_t> decryptPlaintext(
    const std::vector<std::uint8_t>& encrypted,
    const std::array<std::uint8_t, 16>& iv) {
    EVP_CIPHER_CTX* rawCtx = EVP_CIPHER_CTX_new();
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(rawCtx, &EVP_CIPHER_CTX_free);
    EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, kAcDataKey.data(), iv.data());
    EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
    std::vector<std::uint8_t> output(encrypted.size(), 0);
    int outLen1 = 0;
    EVP_DecryptUpdate(ctx.get(), output.data(), &outLen1, encrypted.data(), static_cast<int>(encrypted.size()));
    int outLen2 = 0;
    EVP_DecryptFinal_ex(ctx.get(), output.data() + outLen1, &outLen2);
    output.resize(static_cast<std::size_t>(outLen1 + outLen2));
    return output;
}

void patchChecksum(std::vector<std::uint8_t>& plain) {
    const auto checksumStart = plain.size() - kFooterBytes;
    unsigned int digestSize = 0;
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    ASSERT_EQ(
        EVP_Digest(
            plain.data() + 4,
            plain.size() - 4U - kFooterBytes,
            digest.data(),
            &digestSize,
            EVP_md5(),
            nullptr),
        1);
    ASSERT_EQ(digestSize, 16U);
    std::copy_n(digest.begin(), 16U, plain.begin() + static_cast<std::ptrdiff_t>(checksumStart));
}

std::vector<std::uint8_t> makeEncryptedContainer(
    const std::uint8_t ivSeed,
    const std::vector<std::vector<std::uint8_t>>& records,
    const std::optional<std::uint32_t> firstRecordDesignOffsetOverride = std::nullopt) {
    std::vector<std::uint8_t> plain(kPlainSize, 0);
    std::size_t writeOffset = kHeaderBytes;
    for (const auto& record : records) {
        std::copy(record.begin(), record.end(), plain.begin() + static_cast<std::ptrdiff_t>(writeOffset));
        writeOffset += record.size();
    }

    const std::uint32_t header0 = static_cast<std::uint32_t>(plain.size() - kFooterBytes);
    std::memcpy(plain.data(), &header0, sizeof(header0));
    const auto header12 = firstRecordDesignOffsetOverride.value_or(records.empty() ? 0U : firstMagicOffset(records.front()));
    std::memcpy(plain.data() + 12, &header12, sizeof(header12));
    const auto recordCount = static_cast<std::uint32_t>(records.size());
    std::memcpy(plain.data() + 16, &recordCount, sizeof(recordCount));

    std::fill(plain.end() - 12, plain.end(), static_cast<std::uint8_t>(0x0C));
    patchChecksum(plain);

    std::array<std::uint8_t, 16> iv{};
    for (std::size_t index = 0; index < iv.size(); ++index) {
        iv[index] = static_cast<std::uint8_t>(ivSeed + index);
    }

    const auto cipher = encryptPlaintext(plain, iv);
    std::vector<std::uint8_t> encrypted;
    encrypted.insert(encrypted.end(), iv.begin(), iv.end());
    encrypted.insert(encrypted.end(), cipher.begin(), cipher.end());
    return encrypted;
}

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::filesystem::path makeTempRoot() {
    const auto tempRoot = std::filesystem::temp_directory_path() / "ac6dm-ac-catalog-contract";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);
    return tempRoot;
}

void writeSampleContainers(const std::filesystem::path& root) {
    writeBytes(root / "USER_DATA002", makeEncryptedContainer(0x10, {
        makeRecordPayload(0x10, 0),
        makeRecordPayload(0x10, 1),
    }));
    writeBytes(root / "USER_DATA003", makeEncryptedContainer(0x20, {
        makeRecordPayload(0x20, 0),
    }));
    writeBytes(root / "USER_DATA004", makeEncryptedContainer(0x30, {
        makeRecordPayload(0x30, 0),
        makeRecordPayload(0x30, 1),
        makeRecordPayload(0x30, 2),
    }));
    writeBytes(root / "USER_DATA005", makeEncryptedContainer(0x40, {
        makeRecordPayload(0x40, 0),
    }));
    writeBytes(root / "USER_DATA006", makeEncryptedContainer(0x50, {
        makeRecordPayload(0x50, 0),
        makeRecordPayload(0x50, 1),
        makeRecordPayload(0x50, 2),
        makeRecordPayload(0x50, 3),
    }));
}

}  // namespace

TEST(AcCatalogContractTest, EncryptedBeginEndRecordMapExportsBucketRecords) {
    const auto tempRoot = makeTempRoot();
    writeSampleContainers(tempRoot);

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    ASSERT_EQ(snapshot.catalog.size(), 11U);
    ASSERT_EQ(snapshot.records.size(), 11U);
    EXPECT_TRUE(snapshot.recordMapQualified);
    EXPECT_TRUE(snapshot.editableClonePatch.qualified);
    EXPECT_EQ(snapshot.editableClonePatch.version, "ac-location-copy-v2");

    EXPECT_EQ(snapshot.catalog.front().assetId, "ac:user1:0");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.front().sourceBucket), "user1");
    EXPECT_EQ(snapshot.catalog.front().archiveName, "ARCHIVE-16-0");
    EXPECT_EQ(snapshot.catalog.front().machineName, "FRAME-16-0");
    EXPECT_EQ(snapshot.catalog.front().shareCode, "B1YA3PAJGNTL");
    EXPECT_EQ(snapshot.catalog.front().detailProvenance, "ac.record-ref.aes-cbc.begin-end.v1");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.front().writeCapability), "editable");
    EXPECT_TRUE(snapshot.catalog.front().recordRef.find("offset=20") != std::string::npos);
    EXPECT_TRUE(snapshot.catalog.front().recordRef.find("qualified=1") != std::string::npos);

    const auto& shareLast = snapshot.catalog.back();
    EXPECT_EQ(shareLast.assetId, "ac:share:3");
    EXPECT_EQ(ac6dm::contracts::toString(shareLast.sourceBucket), "share");
    EXPECT_EQ(shareLast.archiveName, "ARCHIVE-80-3");
    EXPECT_EQ(shareLast.machineName, "FRAME-80-3");
    EXPECT_EQ(shareLast.shareCode, "B1YA3PAJGNTL");
    EXPECT_EQ(ac6dm::contracts::toString(shareLast.writeCapability), "read_only");
    EXPECT_EQ(shareLast.slotLabel, "Share / AC 04");

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, GateAc03ShareCopyAppendsOneRecordAndReadbackMatches) {
    const auto tempRoot = makeTempRoot();
    writeSampleContainers(tempRoot);

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    const int targetSlot = 1;
    const auto plan = ac6dm::ac::buildShareAcImportPlan(snapshot, "ac:share:2", targetSlot);
    EXPECT_TRUE(plan.blockers.empty());
    EXPECT_EQ(plan.targetSlot, "User 2 / AC 02");

    const auto sourcePayload = ac6dm::ac::readRecordPayload(tempRoot, "ac:share:2", snapshot);
    const auto copiedPayload = ac6dm::ac::applyShareAcPayloadCopy(tempRoot, snapshot, "ac:share:2", targetSlot);
    EXPECT_NE(copiedPayload, sourcePayload);
    EXPECT_EQ(copiedPayload.size(), sourcePayload.size());

    const auto readback = ac6dm::ac::verifyShareAcPayloadReadback(tempRoot, targetSlot, copiedPayload, "ac:share:2");
    EXPECT_TRUE(readback.payloadMatches);
    EXPECT_EQ(readback.actualPayloadBytes, copiedPayload.size());
    EXPECT_EQ(readback.targetRecordRef.recordIndex, 1);
    EXPECT_TRUE(readback.targetRecordRef.qualified);

    const auto updatedSnapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    const auto updatedPayload = ac6dm::ac::readRecordPayload(tempRoot, "ac:user2:1", updatedSnapshot);
    EXPECT_EQ(updatedPayload, copiedPayload);
    EXPECT_EQ(std::count_if(updatedSnapshot.records.begin(), updatedSnapshot.records.end(), [](const auto& record) {
        return record.sourceBucket == ac6dm::contracts::SourceBucket::User2;
    }), 2);

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, GateAc03RejectsNonShareAndInvalidTargetBucket) {
    const auto tempRoot = makeTempRoot();
    writeSampleContainers(tempRoot);
    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);

    const auto nonSharePlan = ac6dm::ac::buildShareAcImportPlan(snapshot, "ac:user1:0", 0);
    EXPECT_FALSE(nonSharePlan.blockers.empty());

    const auto invalidPlan = ac6dm::ac::buildShareAcImportPlan(snapshot, "ac:share:0", 9999);
    EXPECT_FALSE(invalidPlan.blockers.empty());

    EXPECT_THROW(ac6dm::ac::applyShareAcPayloadCopy(tempRoot, snapshot, "ac:user1:0", 0), ac6dm::ac::AcQualificationError);
    EXPECT_THROW(ac6dm::ac::applyShareAcPayloadCopy(tempRoot, snapshot, "ac:share:0", 9999), ac6dm::ac::AcQualificationError);

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, RejectsGarbageUtf16AtLegacyNameOffsets) {
    const auto tempRoot = makeTempRoot();
    writeBytes(tempRoot / "USER_DATA002", makeEncryptedContainer(0x10, {
        makeGarbageNamedRecordPayload(0x10, 0),
    }));

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    ASSERT_EQ(snapshot.catalog.size(), 1U);
    EXPECT_EQ(snapshot.catalog.front().archiveName, "User 1 Archive 01");
    EXPECT_EQ(snapshot.catalog.front().machineName, "User 1 AC 01");
    EXPECT_TRUE(snapshot.catalog.front().shareCode.empty());

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, ExtractsNamesFromInflatedAsmcDesignPayload) {
    const auto tempRoot = makeTempRoot();
    writeBytes(tempRoot / "USER_DATA002", makeEncryptedContainer(0x10, {
        makeAsmcNamedRecordPayload("MELEE 01", "CYAN FANG", "T8H722ZD45D0"),
    }));

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    ASSERT_EQ(snapshot.catalog.size(), 1U);
    EXPECT_EQ(snapshot.catalog.front().archiveName, "MELEE 01");
    EXPECT_EQ(snapshot.catalog.front().machineName, "CYAN FANG");
    EXPECT_EQ(snapshot.catalog.front().shareCode, "T8H722ZD45D0");

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, PreserveContainerFirstRecordDesignOffsetWhenAppending) {
    const auto tempRoot = makeTempRoot();
    writeBytes(tempRoot / "USER_DATA004", makeEncryptedContainer(0x30, {
        makeRecordPayload(0x30, 0),
    }, 135U));
    writeBytes(tempRoot / "USER_DATA006", makeEncryptedContainer(0x50, {
        makeRecordPayload(0x50, 0),
    }));

    const auto beforeBytes = readBytes(tempRoot / "USER_DATA004");
    ASSERT_GT(beforeBytes.size(), 16U);
    std::array<std::uint8_t, 16> beforeIv{};
    std::copy_n(beforeBytes.begin(), 16, beforeIv.begin());
    const auto beforePlain = decryptPlaintext(
        std::vector<std::uint8_t>(beforeBytes.begin() + 16, beforeBytes.end()),
        beforeIv);
    std::uint32_t beforeFirstRecordDesignOffset = 0;
    std::uint32_t beforeRecordCount = 0;
    std::memcpy(&beforeFirstRecordDesignOffset, beforePlain.data() + 12, sizeof(beforeFirstRecordDesignOffset));
    std::memcpy(&beforeRecordCount, beforePlain.data() + 16, sizeof(beforeRecordCount));
    EXPECT_EQ(beforeFirstRecordDesignOffset, 135U);
    EXPECT_EQ(beforeRecordCount, 1U);

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    const auto payload = ac6dm::ac::readRecordPayload(tempRoot, "ac:share:0", snapshot);
    ac6dm::ac::applyPayloadToUserSlot(tempRoot, payload, 2);

    const auto afterBytes = readBytes(tempRoot / "USER_DATA004");
    ASSERT_GT(afterBytes.size(), 16U);
    std::array<std::uint8_t, 16> afterIv{};
    std::copy_n(afterBytes.begin(), 16, afterIv.begin());
    const auto afterPlain = decryptPlaintext(
        std::vector<std::uint8_t>(afterBytes.begin() + 16, afterBytes.end()),
        afterIv);
    std::uint32_t afterFirstRecordDesignOffset = 0;
    std::uint32_t afterRecordCount = 0;
    std::memcpy(&afterFirstRecordDesignOffset, afterPlain.data() + 12, sizeof(afterFirstRecordDesignOffset));
    std::memcpy(&afterRecordCount, afterPlain.data() + 16, sizeof(afterRecordCount));
    EXPECT_EQ(afterFirstRecordDesignOffset, 135U);
    EXPECT_EQ(afterRecordCount, 2U);

    const auto rebuiltSnapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    const auto rebuiltUser3Count = std::count_if(
        rebuiltSnapshot.records.begin(),
        rebuiltSnapshot.records.end(),
        [](const auto& record) {
            return record.sourceBucket == ac6dm::contracts::SourceBucket::User3;
        });
    EXPECT_EQ(rebuiltUser3Count, 2);

    std::filesystem::remove_all(tempRoot);
}

TEST(AcCatalogContractTest, ShareImportPatchesCategoryToTargetBucketAndChecksum) {
    const auto tempRoot = makeTempRoot();
    writeBytes(tempRoot / "USER_DATA004", makeEncryptedContainer(0x30, {
        makeRecordPayload(0x03, 0),
    }, 135U));
    writeBytes(tempRoot / "USER_DATA006", makeEncryptedContainer(0x70, {
        makeRecordPayload(0x07, 0),
    }));

    const auto snapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(tempRoot);
    const auto expectedPayload = ac6dm::ac::applyShareAcPayloadCopy(tempRoot, snapshot, "ac:share:0", 2);
    ASSERT_EQ(expectedPayload.at(64), 0x03);

    const auto readback = ac6dm::ac::verifyShareAcPayloadReadback(tempRoot, 2, expectedPayload, "ac:share:0");
    EXPECT_TRUE(readback.payloadMatches);

    const auto targetBytes = readBytes(tempRoot / "USER_DATA004");
    ASSERT_GT(targetBytes.size(), 16U);
    std::array<std::uint8_t, 16> iv{};
    std::copy_n(targetBytes.begin(), 16, iv.begin());
    const auto plain = decryptPlaintext(
        std::vector<std::uint8_t>(targetBytes.begin() + 16, targetBytes.end()),
        iv);
    const auto checksumStart = plain.size() - kFooterBytes;
    const auto checksum = std::vector<std::uint8_t>(plain.begin() + static_cast<std::ptrdiff_t>(checksumStart),
                                                    plain.begin() + static_cast<std::ptrdiff_t>(checksumStart + 16));
    unsigned int digestSize = 0;
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    ASSERT_EQ(
        EVP_Digest(
            plain.data() + 4,
            plain.size() - 4U - kFooterBytes,
            digest.data(),
            &digestSize,
            EVP_md5(),
            nullptr),
        1);
    ASSERT_EQ(digestSize, 16U);
    EXPECT_TRUE(std::equal(checksum.begin(), checksum.end(), digest.begin()));

    std::filesystem::remove_all(tempRoot);
}
