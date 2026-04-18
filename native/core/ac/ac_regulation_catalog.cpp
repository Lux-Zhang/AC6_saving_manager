#include "ac_regulation_catalog.hpp"

#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>
#include <unordered_map>

namespace ac6dm::ac {
namespace {

struct RegulationPresetCatalogStorage final {
    RegulationPresetCatalog catalog;
    std::unordered_map<int, std::size_t> charaIndex;
    std::unordered_map<int, std::size_t> arenaIndex;
};

QJsonDocument readJsonResource(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("failed to open regulation preset resource: " + path.toStdString());
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (document.isNull() || !document.isObject()) {
        throw std::runtime_error("failed to parse regulation preset resource: " + path.toStdString());
    }
    return document;
}

RegulationPresetCatalogStorage loadCatalog() {
    RegulationPresetCatalogStorage storage;
    const auto root = readJsonResource(QStringLiteral(":/advanced_garage/data/regulation_preset_catalog.json")).object();

    storage.catalog.generatedAt = root.value(QStringLiteral("generatedAt")).toString().toStdString();
    storage.catalog.source = root.value(QStringLiteral("source")).toString().toStdString();
    storage.catalog.runtimeMetadata.validatedUserData010PlainSha256 =
        root.value(QStringLiteral("validatedUserData010PlainSha256")).toString().toStdString();
    storage.catalog.runtimeMetadata.validatedUserData010PlainLength =
        root.value(QStringLiteral("validatedUserData010PlainLength")).toVariant().toULongLong();
    storage.catalog.runtimeMetadata.validatedUserData010EmbeddedOffset =
        static_cast<std::uint32_t>(root.value(QStringLiteral("validatedUserData010EmbeddedOffset")).toInt(20));
    storage.catalog.runtimeMetadata.validatedUserData010HeaderMagic =
        root.value(QStringLiteral("validatedUserData010HeaderMagic")).toString().toStdString();
    storage.catalog.runtimeMetadata.validatedUserData010Version =
        static_cast<std::uint32_t>(root.value(QStringLiteral("validatedUserData010Version")).toInt(0));

    const auto charaRows = root.value(QStringLiteral("charaRows")).toArray();
    storage.catalog.charaRows.reserve(static_cast<std::size_t>(charaRows.size()));
    for (const auto& value : charaRows) {
        const auto object = value.toObject();
        RegulationCharaEquipRow row;
        row.charaId = object.value(QStringLiteral("id")).toInt(-1);
        row.name = object.value(QStringLiteral("name")).toString().toStdString();
        const auto equipObject = object.value(QStringLiteral("equip")).toObject();
        for (auto equipIt = equipObject.begin(); equipIt != equipObject.end(); ++equipIt) {
            row.equipFields.emplace(equipIt.key().toStdString(), equipIt.value().toInt());
        }
        storage.charaIndex.emplace(row.charaId, storage.catalog.charaRows.size());
        storage.catalog.charaRows.push_back(std::move(row));
    }

    const auto arenaRows = root.value(QStringLiteral("arenaRows")).toArray();
    storage.catalog.arenaRows.reserve(static_cast<std::size_t>(arenaRows.size()));
    for (const auto& value : arenaRows) {
        const auto object = value.toObject();
        RegulationArenaLinkRow row;
        row.arenaId = object.value(QStringLiteral("id")).toInt(-1);
        row.name = object.value(QStringLiteral("name")).toString().toStdString();
        row.accountId = object.value(QStringLiteral("accountId")).toInt(-1);
        row.charaId = object.value(QStringLiteral("charaId")).toInt(-1);
        row.npcId = object.value(QStringLiteral("npcId")).toInt(-1);
        row.thinkId = object.value(QStringLiteral("thinkId")).toInt(-1);
        storage.arenaIndex.emplace(row.arenaId, storage.catalog.arenaRows.size());
        storage.catalog.arenaRows.push_back(std::move(row));
    }

    return storage;
}

const RegulationPresetCatalogStorage& catalogStorage() {
    static const auto storage = loadCatalog();
    return storage;
}

}  // namespace

const RegulationPresetCatalog& regulationPresetCatalog() {
    return catalogStorage().catalog;
}

const RegulationCharaEquipRow* findRegulationCharaRow(const int charaId) {
    const auto& storage = catalogStorage();
    const auto it = storage.charaIndex.find(charaId);
    if (it == storage.charaIndex.end()) {
        return nullptr;
    }
    return &storage.catalog.charaRows[it->second];
}

const RegulationArenaLinkRow* findRegulationArenaRow(const int arenaId) {
    const auto& storage = catalogStorage();
    const auto it = storage.arenaIndex.find(arenaId);
    if (it == storage.arenaIndex.end()) {
        return nullptr;
    }
    return &storage.catalog.arenaRows[it->second];
}

const RegulationCharaEquipRow* findRegulationArenaResolvedCharaRow(const int arenaId) {
    const auto* arenaRow = findRegulationArenaRow(arenaId);
    if (arenaRow == nullptr) {
        return nullptr;
    }
    return findRegulationCharaRow(arenaRow->charaId);
}

std::optional<int> findRegulationCharaEquipValue(
    const RegulationCharaEquipRow& row,
    const std::string_view fieldName) {
    const auto it = row.equipFields.find(std::string(fieldName));
    if (it == row.equipFields.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace ac6dm::ac
