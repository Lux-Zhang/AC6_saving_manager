#include "package_codec.hpp"

#include "ac6dm/contracts/slot_contracts.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <stdexcept>

namespace ac6dm::exchange {
namespace {

using ac6dm::contracts::AssetKind;
using ac6dm::contracts::CatalogItemDto;
using ac6dm::contracts::ExchangeAssetKind;
using ac6dm::contracts::PreviewState;
using ac6dm::contracts::SourceBucket;
using ac6dm::contracts::WriteCapability;

QString readAllText(const std::filesystem::path& path) {
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open exchange file: " + path.generic_string());
    }
    return QString::fromUtf8(file.readAll());
}

void writeAllText(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error("Failed to write exchange file: " + path.generic_string());
    }
    file.write(bytes);
}

std::string toString(const ExchangeAssetKind assetKind) {
    return assetKind == ExchangeAssetKind::Emblem ? "emblem" : "ac";
}

ExchangeAssetKind assetKindFromString(const std::string& value) {
    if (value == "emblem") {
        return ExchangeAssetKind::Emblem;
    }
    if (value == "ac") {
        return ExchangeAssetKind::Ac;
    }
    throw std::runtime_error("Unknown exchange asset kind: " + value);
}

SourceBucket sourceBucketFromString(const std::string& value) {
    if (value == "user1") {
        return SourceBucket::User1;
    }
    if (value == "user2") {
        return SourceBucket::User2;
    }
    if (value == "user3") {
        return SourceBucket::User3;
    }
    if (value == "user4") {
        return SourceBucket::User4;
    }
    if (value == "share") {
        return SourceBucket::Share;
    }
    throw std::runtime_error("Unknown source bucket: " + value);
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
    throw std::runtime_error("Unknown preview state: " + value);
}

std::string requiredExtensionFor(const ExchangeAssetKind assetKind) {
    return assetKind == ExchangeAssetKind::Emblem
        ? std::string(ac6dm::contracts::kAc6EmblemDataExtension)
        : std::string(ac6dm::contracts::kAc6AcDataExtension);
}

void validatePathExtension(const std::filesystem::path& path, const ExchangeAssetKind assetKind) {
    const auto expected = requiredExtensionFor(assetKind);
    if (path.extension().string() != expected) {
        throw std::runtime_error(
            "Exchange file extension mismatch: expected " + expected + ", got " + path.extension().string());
    }
}

QJsonObject toJson(const ExchangePackage& package) {
    QJsonObject root;
    root.insert("format_version", QString::fromStdString(package.formatVersion));
    root.insert("asset_kind", QString::fromStdString(toString(package.assetKind)));
    root.insert("title", QString::fromStdString(package.title));
    root.insert("archive_name", QString::fromStdString(package.archiveName));
    root.insert("machine_name", QString::fromStdString(package.machineName));
    root.insert("share_code", QString::fromStdString(package.shareCode));
    root.insert("source_bucket", QString::fromStdString(package.sourceBucket));
    root.insert("record_ref", QString::fromStdString(package.recordRef));
    root.insert("preview_state", QString::fromStdString(package.previewState));
    root.insert("preview_provenance", QString::fromStdString(package.previewProvenance));
    root.insert(
        "record_payload_base64",
        QString::fromLatin1(QByteArray(
                                reinterpret_cast<const char*>(package.recordPayload.data()),
                                static_cast<int>(package.recordPayload.size()))
                                .toBase64()));
    root.insert("checksum_sha256", QString::fromStdString(package.checksumSha256));
    root.insert("producer", QString::fromStdString(package.producer));
    root.insert("min_reader_version", QString::fromStdString(package.minReaderVersion));
    root.insert("payload_schema", QString::fromStdString(package.payloadSchema));
    root.insert("editable_clone_patch_version", QString::fromStdString(package.editableClonePatchVersion));
    if (package.previewBytes.has_value()) {
        root.insert(
            "preview_bytes_base64",
            QString::fromLatin1(QByteArray(
                                    reinterpret_cast<const char*>(package.previewBytes->data()),
                                    static_cast<int>(package.previewBytes->size()))
                                    .toBase64()));
    }
    return root;
}

std::vector<std::uint8_t> decodeBase64Field(const QJsonObject& root, const char* key, bool required) {
    const auto value = root.value(QString::fromLatin1(key));
    if (value.isUndefined() || value.isNull()) {
        if (required) {
            throw std::runtime_error(std::string("Missing required field: ") + key);
        }
        return {};
    }
    const auto bytes = QByteArray::fromBase64(value.toString().toLatin1());
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

std::string requireStringField(const QJsonObject& root, const char* key) {
    const auto value = root.value(QString::fromLatin1(key));
    if (!value.isString()) {
        throw std::runtime_error(std::string("Missing or invalid string field: ") + key);
    }
    return value.toString().toStdString();
}

std::string optionalStringField(const QJsonObject& root, const char* key) {
    const auto value = root.value(QString::fromLatin1(key));
    return value.isString() ? value.toString().toStdString() : std::string{};
}

}  // namespace

std::string defaultPayloadSchemaFor(const ExchangeAssetKind assetKind) {
    return assetKind == ExchangeAssetKind::Emblem ? "emblem.embc.raw.v1" : "ac.record.raw.v1";
}

std::string computePayloadSha256(const std::vector<std::uint8_t>& payload) {
    const auto hash = QCryptographicHash::hash(
        QByteArray(reinterpret_cast<const char*>(payload.data()), static_cast<int>(payload.size())),
        QCryptographicHash::Sha256);
    return hash.toHex().toStdString();
}

void writeExchangePackage(const std::filesystem::path& outputPath, const ExchangePackage& package) {
    validatePathExtension(outputPath, package.assetKind);
    const auto expectedChecksum = computePayloadSha256(package.recordPayload);
    if (expectedChecksum != package.checksumSha256) {
        throw std::runtime_error("Refusing to write exchange package with mismatched payload checksum");
    }
    const auto document = QJsonDocument(toJson(package));
    writeAllText(outputPath, document.toJson(QJsonDocument::Indented));
}

ExchangePackage readExchangePackage(const std::filesystem::path& inputPath) {
    const auto document = QJsonDocument::fromJson(readAllText(inputPath).toUtf8());
    if (!document.isObject()) {
        throw std::runtime_error("Exchange file is not a valid JSON object");
    }
    const auto root = document.object();

    ExchangePackage package;
    package.formatVersion = requireStringField(root, "format_version");
    package.assetKind = assetKindFromString(requireStringField(root, "asset_kind"));
    package.title = requireStringField(root, "title");
    package.archiveName = optionalStringField(root, "archive_name");
    package.machineName = optionalStringField(root, "machine_name");
    package.shareCode = optionalStringField(root, "share_code");
    package.sourceBucket = requireStringField(root, "source_bucket");
    package.recordRef = requireStringField(root, "record_ref");
    package.previewState = requireStringField(root, "preview_state");
    package.previewProvenance = requireStringField(root, "preview_provenance");
    package.recordPayload = decodeBase64Field(root, "record_payload_base64", true);
    package.checksumSha256 = requireStringField(root, "checksum_sha256");
    package.producer = requireStringField(root, "producer");
    package.minReaderVersion = requireStringField(root, "min_reader_version");
    package.payloadSchema = requireStringField(root, "payload_schema");
    package.editableClonePatchVersion = requireStringField(root, "editable_clone_patch_version");
    const auto previewBytes = decodeBase64Field(root, "preview_bytes_base64", false);
    if (!previewBytes.empty()) {
        package.previewBytes = previewBytes;
    }

    if (package.formatVersion != std::string(ac6dm::contracts::kExchangeFormatVersionV1)) {
        throw std::runtime_error("Unsupported exchange format version: " + package.formatVersion);
    }
    validatePathExtension(inputPath, package.assetKind);
    if (package.checksumSha256 != computePayloadSha256(package.recordPayload)) {
        throw std::runtime_error("Exchange file checksum mismatch");
    }
    return package;
}

CatalogItemDto inspectExchangePackage(const std::filesystem::path& inputPath) {
    const auto package = readExchangePackage(inputPath);
    CatalogItemDto item;
    item.assetId = inputPath.filename().string();
    item.assetKind = package.assetKind == ExchangeAssetKind::Emblem ? AssetKind::Emblem : AssetKind::Ac;
    item.sourceBucket = sourceBucketFromString(package.sourceBucket);
    item.slotIndex = ac6dm::contracts::slotIndexOf(item.sourceBucket);
    item.archiveName = package.archiveName;
    item.machineName = package.machineName;
    item.shareCode = package.shareCode;
    item.displayName = !package.title.empty()
        ? package.title
        : !item.archiveName.empty()
            ? item.archiveName
            : !item.machineName.empty()
                ? item.machineName
                : item.shareCode;
    item.previewState = previewStateFromString(package.previewState);
    item.writeCapability = item.assetKind == AssetKind::Emblem
        ? WriteCapability::Editable
        : WriteCapability::LockedPendingGate;
    item.recordRef = package.recordRef;
    item.detailProvenance = package.payloadSchema.empty() ? "exchange-package-v1" : package.payloadSchema;
    item.slotLabel = item.slotIndex.has_value()
        ? ac6dm::contracts::formatUserSlotLabel(*item.slotIndex)
        : "SHARE";
    item.origin = ac6dm::contracts::AssetOrigin::Package;
    item.editable = item.assetKind == AssetKind::Emblem;
    item.sourceLabel = ac6dm::contracts::toDisplayLabel(item.sourceBucket);
    item.description = std::string("外部交换文件：") + inputPath.filename().string();
    item.tags = {"package", toString(package.assetKind), package.payloadSchema};
    item.detailFields.push_back({"Title", item.displayName});
    item.detailFields.push_back({"ArchiveName", item.archiveName});
    item.detailFields.push_back({"AcName", item.machineName});
    item.detailFields.push_back({"ShareCode", item.shareCode});
    item.detailFields.push_back({"AssetKind", ac6dm::contracts::toString(item.assetKind)});
    item.detailFields.push_back({"SourceBucket", ac6dm::contracts::toString(item.sourceBucket)});
    item.detailFields.push_back({"PreviewState", ac6dm::contracts::toString(item.previewState)});
    item.detailFields.push_back({"RecordRef", item.recordRef});
    item.detailFields.push_back({"Producer", package.producer});
    item.detailFields.push_back({"PayloadSchema", package.payloadSchema});
    item.detailFields.push_back({"EditableClonePatchVersion", package.editableClonePatchVersion});
    item.detailFields.push_back({"ChecksumSha256", package.checksumSha256});
    item.detailFields.push_back({"FormatVersion", package.formatVersion});
    item.detailFields.push_back({"PreviewProvenance", package.previewProvenance});
    item.preview.provenance = package.previewProvenance;
    return item;
}

}  // namespace ac6dm::exchange
