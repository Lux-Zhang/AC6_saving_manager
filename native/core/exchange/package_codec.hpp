#pragma once

#include "ac6dm/contracts/asset_catalog.hpp"
#include "ac6dm/contracts/exchange_contracts.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::exchange {

struct ExchangePackage final {
    std::string formatVersion;
    contracts::ExchangeAssetKind assetKind{contracts::ExchangeAssetKind::Emblem};
    std::string title;
    std::string archiveName;
    std::string machineName;
    std::string shareCode;
    std::string sourceBucket;
    std::string recordRef;
    std::string previewState;
    std::string previewProvenance;
    std::optional<std::vector<std::uint8_t>> previewBytes;
    std::vector<std::uint8_t> recordPayload;
    std::string checksumSha256;
    std::string producer;
    std::string minReaderVersion;
    std::string payloadSchema;
    std::string editableClonePatchVersion;
};

std::string defaultPayloadSchemaFor(contracts::ExchangeAssetKind assetKind);
std::string computePayloadSha256(const std::vector<std::uint8_t>& payload);

void writeExchangePackage(const std::filesystem::path& outputPath, const ExchangePackage& package);
ExchangePackage readExchangePackage(const std::filesystem::path& inputPath);
contracts::CatalogItemDto inspectExchangePackage(const std::filesystem::path& inputPath);

}  // namespace ac6dm::exchange
