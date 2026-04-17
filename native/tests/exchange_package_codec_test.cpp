#include "core/exchange/package_codec.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

ac6dm::exchange::ExchangePackage makePackage(ac6dm::contracts::ExchangeAssetKind assetKind) {
    ac6dm::exchange::ExchangePackage package;
    package.formatVersion = "1";
    package.assetKind = assetKind;
    package.title = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem ? "Steel Haze Mark" : "Steel Haze AC";
    package.archiveName = assetKind == ac6dm::contracts::ExchangeAssetKind::Ac ? "AUREATE CAVALRY" : "";
    package.machineName = assetKind == ac6dm::contracts::ExchangeAssetKind::Ac ? "JIN GE" : "";
    package.shareCode = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem ? "EFS09D2FJ66M" : "B1YA3PAJGNTL";
    package.sourceBucket = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem ? "share" : "user1";
    package.recordRef = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem
        ? "USER_DATA007/extraFiles/0"
        : "USER_DATA002/record/0?bucket=user1&offset=0&size=7&stable=test-user1&qualified=0";
    package.previewState = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem ? "derived_render" : "unknown";
    package.previewProvenance = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem
        ? "emblem.qt-basic-shapes-v1"
        : "ac.record-preview-forensics-only";
    package.recordPayload = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem
        ? std::vector<std::uint8_t>{0x45, 0x4D, 0x42, 0x43, 0x00, 0x01}
        : std::vector<std::uint8_t>{0x55, 0x44, 0x30, 0x30, 0x32, 0xAA, 0xBB};
    package.checksumSha256 = ac6dm::exchange::computePayloadSha256(package.recordPayload);
    package.producer = "ac6dm-native-cpp/0.0.1";
    package.minReaderVersion = "0.0.1";
    package.payloadSchema = ac6dm::exchange::defaultPayloadSchemaFor(assetKind);
    package.editableClonePatchVersion = assetKind == ac6dm::contracts::ExchangeAssetKind::Emblem
        ? "emblem-category-patch-v1"
        : "ac-location-copy-v2";
    return package;
}

std::filesystem::path makeTempPath(const std::string& filename) {
    const auto root = std::filesystem::temp_directory_path() / "ac6dm-exchange-tests";
    std::filesystem::create_directories(root);
    return root / filename;
}

}  // namespace

TEST(ExchangePackageCodecTest, EmblemRoundTripPreservesStableFields) {
    const auto path = makeTempPath("sample.ac6emblemdata");
    const auto package = makePackage(ac6dm::contracts::ExchangeAssetKind::Emblem);

    ac6dm::exchange::writeExchangePackage(path, package);
    const auto readback = ac6dm::exchange::readExchangePackage(path);

    EXPECT_EQ(readback.assetKind, ac6dm::contracts::ExchangeAssetKind::Emblem);
    EXPECT_EQ(readback.title, package.title);
    EXPECT_EQ(readback.sourceBucket, "share");
    EXPECT_EQ(readback.recordRef, "USER_DATA007/extraFiles/0");
    EXPECT_EQ(readback.previewState, "derived_render");
    EXPECT_EQ(readback.shareCode, "EFS09D2FJ66M");
    EXPECT_EQ(readback.payloadSchema, "emblem.embc.raw.v1");
    EXPECT_EQ(readback.editableClonePatchVersion, "emblem-category-patch-v1");
    EXPECT_EQ(readback.recordPayload, package.recordPayload);
    EXPECT_FALSE(readback.previewBytes.has_value());
}

TEST(ExchangePackageCodecTest, AcRoundTripPreservesStableFields) {
    const auto path = makeTempPath("sample.ac6acdata");
    const auto package = makePackage(ac6dm::contracts::ExchangeAssetKind::Ac);

    ac6dm::exchange::writeExchangePackage(path, package);
    const auto readback = ac6dm::exchange::readExchangePackage(path);

    EXPECT_EQ(readback.assetKind, ac6dm::contracts::ExchangeAssetKind::Ac);
    EXPECT_EQ(readback.title, package.title);
    EXPECT_EQ(readback.sourceBucket, "user1");
    EXPECT_EQ(readback.recordRef, "USER_DATA002/record/0?bucket=user1&offset=0&size=7&stable=test-user1&qualified=0");
    EXPECT_EQ(readback.previewState, "unknown");
    EXPECT_EQ(readback.archiveName, "AUREATE CAVALRY");
    EXPECT_EQ(readback.machineName, "JIN GE");
    EXPECT_EQ(readback.shareCode, "B1YA3PAJGNTL");
    EXPECT_EQ(readback.payloadSchema, ac6dm::exchange::defaultPayloadSchemaFor(ac6dm::contracts::ExchangeAssetKind::Ac));
    EXPECT_EQ(readback.editableClonePatchVersion, "ac-location-copy-v2");
    EXPECT_EQ(readback.recordPayload, package.recordPayload);
    EXPECT_FALSE(readback.previewBytes.has_value());
}

TEST(ExchangePackageCodecTest, InspectSummaryExportsCatalogShape) {
    const auto path = makeTempPath("inspect.ac6emblemdata");
    const auto package = makePackage(ac6dm::contracts::ExchangeAssetKind::Emblem);
    ac6dm::exchange::writeExchangePackage(path, package);

    const auto item = ac6dm::exchange::inspectExchangePackage(path);
    EXPECT_EQ(item.assetKind, ac6dm::contracts::AssetKind::Emblem);
    EXPECT_EQ(item.sourceBucket, ac6dm::contracts::SourceBucket::Share);
    EXPECT_EQ(item.displayName, "Steel Haze Mark");
    EXPECT_EQ(item.shareCode, "EFS09D2FJ66M");
    EXPECT_EQ(item.previewState, ac6dm::contracts::PreviewState::DerivedRender);
    EXPECT_EQ(item.origin, ac6dm::contracts::AssetOrigin::Package);
    EXPECT_EQ(item.detailProvenance, "emblem.embc.raw.v1");
    EXPECT_EQ(item.preview.provenance, "emblem.qt-basic-shapes-v1");
}

TEST(ExchangePackageCodecTest, ChecksumMismatchFailsClosed) {
    const auto path = makeTempPath("checksum-mismatch.ac6emblemdata");
    auto package = makePackage(ac6dm::contracts::ExchangeAssetKind::Emblem);
    ac6dm::exchange::writeExchangePackage(path, package);

    auto contents = std::string{};
    {
        std::ifstream input(path);
        contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    const auto needle = package.checksumSha256;
    const auto pos = contents.find(needle);
    ASSERT_NE(pos, std::string::npos);
    contents.replace(pos, needle.size(), std::string(needle.size(), '0'));
    {
        std::ofstream output(path, std::ios::trunc);
        output << contents;
    }

    EXPECT_THROW((void)ac6dm::exchange::readExchangePackage(path), std::runtime_error);
}

TEST(ExchangePackageCodecTest, UnsupportedFormatVersionFailsClosed) {
    const auto path = makeTempPath("unsupported-version.ac6emblemdata");
    auto package = makePackage(ac6dm::contracts::ExchangeAssetKind::Emblem);
    package.formatVersion = "999";
    package.checksumSha256 = ac6dm::exchange::computePayloadSha256(package.recordPayload);

    auto contents = QJsonDocument(QJsonObject{
        {"format_version", QString::fromStdString(package.formatVersion)},
        {"asset_kind", "emblem"},
        {"title", QString::fromStdString(package.title)},
        {"source_bucket", QString::fromStdString(package.sourceBucket)},
        {"record_ref", QString::fromStdString(package.recordRef)},
        {"preview_state", QString::fromStdString(package.previewState)},
        {"preview_provenance", QString::fromStdString(package.previewProvenance)},
        {"record_payload_base64", QString::fromLatin1(QByteArray(
            reinterpret_cast<const char*>(package.recordPayload.data()),
            static_cast<int>(package.recordPayload.size())).toBase64())},
        {"checksum_sha256", QString::fromStdString(package.checksumSha256)},
        {"producer", QString::fromStdString(package.producer)},
        {"min_reader_version", QString::fromStdString(package.minReaderVersion)},
        {"payload_schema", QString::fromStdString(package.payloadSchema)},
        {"editable_clone_patch_version", QString::fromStdString(package.editableClonePatchVersion)},
    }).toJson(QJsonDocument::Indented);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(contents.constData(), static_cast<std::streamsize>(contents.size()));
    }

    EXPECT_THROW((void)ac6dm::exchange::readExchangePackage(path), std::runtime_error);
}
