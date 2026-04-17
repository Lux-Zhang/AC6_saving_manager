#include "app/native_exchange_workflow.hpp"
#include "core/ac/ac_catalog.hpp"
#include "core/exchange/package_codec.hpp"

#include <openssl/evp.h>
#include <gtest/gtest.h>

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
constexpr std::size_t kFooterBytes = 16;
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

std::vector<std::uint8_t> makeRecordPayload(const std::uint8_t seed, const int ordinal) {
    std::vector<std::uint8_t> bytes;
    appendDelimiter(bytes, "---- begin ----");
    appendBlock(bytes, "Category", {static_cast<std::uint8_t>(seed + ordinal)});
    appendBlock(bytes, "DataName", utf16LeNullTerminated("ARCHIVE-" + std::to_string(seed) + "-" + std::to_string(ordinal)));
    appendBlock(bytes, "AcName", utf16LeNullTerminated("FRAME-" + std::to_string(seed) + "-" + std::to_string(ordinal)));
    appendBlock(bytes, "UgcID", utf16LeNullTerminated("CODE-" + std::to_string(seed) + "-" + std::to_string(ordinal)));
    appendBlock(bytes, "DateTime", std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(seed + ordinal)));

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

std::vector<std::uint8_t> makeEncryptedContainer(
    const std::uint8_t ivSeed,
    const std::vector<std::vector<std::uint8_t>>& records) {
    std::vector<std::uint8_t> plain(kPlainSize, 0);
    std::size_t writeOffset = kHeaderBytes;
    for (const auto& record : records) {
        std::copy(record.begin(), record.end(), plain.begin() + static_cast<std::ptrdiff_t>(writeOffset));
        writeOffset += record.size();
    }

    const std::uint32_t header0 = static_cast<std::uint32_t>(plain.size() - kFooterBytes);
    std::memcpy(plain.data(), &header0, sizeof(header0));
    const auto header12 = records.empty() ? 0U : firstMagicOffset(records.front());
    std::memcpy(plain.data() + 12, &header12, sizeof(header12));
    const auto recordCount = static_cast<std::uint32_t>(records.size());
    std::memcpy(plain.data() + 16, &recordCount, sizeof(recordCount));

    plain[plain.size() - 16] = ivSeed;
    plain[plain.size() - 15] = static_cast<std::uint8_t>(ivSeed + 1);
    plain[plain.size() - 14] = static_cast<std::uint8_t>(ivSeed + 2);
    plain[plain.size() - 13] = static_cast<std::uint8_t>(ivSeed + 3);
    std::fill(plain.end() - 12, plain.end(), static_cast<std::uint8_t>(0x0C));

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

std::filesystem::path makeTempRoot(const std::string& name) {
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
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
    }));
    writeBytes(root / "USER_DATA005", makeEncryptedContainer(0x40, {
        makeRecordPayload(0x40, 0),
    }));
    writeBytes(root / "USER_DATA006", makeEncryptedContainer(0x50, {
        makeRecordPayload(0x50, 0),
        makeRecordPayload(0x50, 1),
        makeRecordPayload(0x50, 2),
    }));
}

ac6dm::contracts::CatalogItemDto makeEmblemCatalogItem() {
    ac6dm::contracts::CatalogItemDto item;
    item.assetId = "share-emblem";
    item.assetKind = ac6dm::contracts::AssetKind::Emblem;
    item.sourceBucket = ac6dm::contracts::SourceBucket::Share;
    item.displayName = "EFS09D2FJ66M";
    item.shareCode = "EFS09D2FJ66M";
    item.previewState = ac6dm::contracts::PreviewState::Unknown;
    item.writeCapability = ac6dm::contracts::WriteCapability::ReadOnly;
    item.recordRef = "USER_DATA007/extraFiles/0";
    item.detailProvenance = "emblem-record-raw-v1";
    item.slotLabel = "SHARE";
    item.origin = ac6dm::contracts::AssetOrigin::Share;
    item.sourceLabel = "share";
    item.description = "share emblem";
    item.preview.provenance = "preview-disabled.temporarily";
    return item;
}

ac6dm::emblem::EmblemSelection makeEmblemSelection() {
    ac6dm::emblem::EmblemSelection selection;
    selection.assetId = "share-emblem";
    selection.sourceBucket = "share";
    selection.fileIndex = 0;
    selection.rawPayload = {'E', 'M', 'B', 'C', 0x00, 0x01};
    return selection;
}

}  // namespace

TEST(NativeExchangeWorkflowTest, ExportEmblemWritesAc6EmblemDataPackage) {
    auto state = std::make_shared<ac6dm::app::NativeWorkflowState>();
    ac6dm::emblem::EmblemCatalogSnapshot snapshot;
    snapshot.catalog.push_back(makeEmblemCatalogItem());
    snapshot.selections.push_back(makeEmblemSelection());
    state->snapshot = snapshot;

    ac6dm::app::NativeExchangeService service(state);
    const auto outputDir = makeTempRoot("ac6dm-native-exchange-emblem");
    const auto outputPath = outputDir / "share-mark.ac6emblemdata";

    const auto result = service.exportAsset("share-emblem", outputPath);

    EXPECT_EQ(result.code, ac6dm::contracts::ResultCode::Ok);
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    const auto package = ac6dm::exchange::readExchangePackage(outputPath);
    EXPECT_EQ(package.assetKind, ac6dm::contracts::ExchangeAssetKind::Emblem);
    EXPECT_EQ(package.title, "EFS09D2FJ66M");
    EXPECT_EQ(package.shareCode, "EFS09D2FJ66M");
    EXPECT_FALSE(package.previewBytes.has_value());
    EXPECT_EQ(package.recordPayload, std::vector<std::uint8_t>({'E', 'M', 'B', 'C', 0x00, 0x01}));
    EXPECT_EQ(package.editableClonePatchVersion, "emblem-category-patch-v1");
}

TEST(NativeExchangeWorkflowTest, ExportAcWritesAc6AcDataPackage) {
    auto state = std::make_shared<ac6dm::app::NativeWorkflowState>();
    const auto unpackedDir = makeTempRoot("ac6dm-native-exchange-ac");
    writeSampleContainers(unpackedDir);

    state->acSnapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(unpackedDir);
    state->unpackedDir = unpackedDir;
    const auto shareItem = std::find_if(
        state->acSnapshot->catalog.begin(),
        state->acSnapshot->catalog.end(),
        [](const auto& item) { return item.assetId == "ac:share:2"; });
    ASSERT_NE(shareItem, state->acSnapshot->catalog.end());

    ac6dm::app::NativeExchangeService service(state);
    const auto outputPath = unpackedDir / "share-ac.ac6acdata";

    const auto result = service.exportAsset(shareItem->assetId, outputPath);

    EXPECT_EQ(result.code, ac6dm::contracts::ResultCode::Ok);
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    const auto package = ac6dm::exchange::readExchangePackage(outputPath);
    EXPECT_EQ(package.assetKind, ac6dm::contracts::ExchangeAssetKind::Ac);
    EXPECT_EQ(package.archiveName, "ARCHIVE-80-2");
    EXPECT_EQ(package.machineName, "FRAME-80-2");
    EXPECT_EQ(package.shareCode, "B1YA3PAJGNTL");
    EXPECT_EQ(package.editableClonePatchVersion, "ac-location-copy-v2");
    EXPECT_EQ(package.recordPayload, ac6dm::ac::readRecordPayload(unpackedDir, "ac:share:2", *state->acSnapshot));
}

TEST(NativeExchangeWorkflowTest, AcExchangeImportPlanAcceptsQualifiedRecordModel) {
    auto state = std::make_shared<ac6dm::app::NativeWorkflowState>();
    const auto unpackedDir = makeTempRoot("ac6dm-native-exchange-plan");
    writeSampleContainers(unpackedDir);
    state->acSnapshot = ac6dm::ac::buildProvisionalCatalogSnapshot(unpackedDir);
    state->container = ac6dm::emblem::UserDataContainer{};

    ac6dm::exchange::ExchangePackage package;
    package.formatVersion = "1";
    package.assetKind = ac6dm::contracts::ExchangeAssetKind::Ac;
    package.title = "Steel Haze AC";
    package.sourceBucket = "share";
    package.recordRef = "USER_DATA006/record/2?bucket=share&offset=240&size=208&stable=test-share-02&qualified=1";
    package.previewState = "unknown";
    package.previewProvenance = "ac.record-preview-forensics-only";
    package.recordPayload = makeRecordPayload(0x7A, 7);
    package.checksumSha256 = ac6dm::exchange::computePayloadSha256(package.recordPayload);
    package.producer = "ac6dm-native-cpp/0.0.1";
    package.minReaderVersion = "0.0.1";
    package.payloadSchema = ac6dm::exchange::defaultPayloadSchemaFor(package.assetKind);
    package.editableClonePatchVersion = "ac-location-copy-v2";

    const auto packagePath = unpackedDir / "steel-haze.ac6acdata";
    ac6dm::exchange::writeExchangePackage(packagePath, package);

    ac6dm::app::NativeExchangeService service(state);
    const auto plan = service.buildExchangeImportPlan(packagePath, 1);

    EXPECT_TRUE(plan.blockers.empty());
    EXPECT_EQ(plan.mode, "ac-exchange-import");
    EXPECT_EQ(plan.targetSlot, "User 2 / AC 02");
    EXPECT_FALSE(plan.targetChoices.empty());
}
