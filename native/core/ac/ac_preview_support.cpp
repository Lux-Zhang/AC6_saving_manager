#include "ac_preview_support.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ac6dm::ac {
namespace {

struct AdvancedGaragePartEntry final {
    int id{-1};
    std::string name;
    std::string kind;
    std::string manufacturer;
};

struct PreviewSlotSpec final {
    const char* slotKey;
    const char* groupKey;
    const char* slotLabel;
    int primaryWordIndex{-1};
    int legacyFallbackWordIndex{-1};
};

struct LegacyResolution final {
    int partId{-1};
    bool provisional{false};
};

struct PreviewLookup final {
    std::unordered_map<std::string, int> legacySlotWords;
    std::uint32_t noneSentinel{0xFFFFFFFFU};
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, int>> legacyRawSlotToPartId;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, int>> provisionalLegacyRawSlotToPartId;
    std::unordered_map<std::uint32_t, int> legacyGlobalRowToPartId;
    std::unordered_set<std::uint32_t> ambiguousLegacyGlobalRows;
    std::unordered_map<int, AdvancedGaragePartEntry> partsById;
    std::unordered_map<std::string, int> normalizedNameToPartId;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, int>> namedRowToPartId;
    int unitNothingPartId{-1};
    int boosterNothingPartId{-1};
    int expansionNothingPartId{-1};
};

constexpr std::array<PreviewSlotSpec, 12> kPreviewSlotSpecs{{
    {"rightArm", "unit", "R-ARM UNIT", 9, 9},
    {"leftArm", "unit", "L-ARM UNIT", 8, 8},
    {"rightBack", "unit", "R-BACK UNIT", 11, 11},
    {"leftBack", "unit", "L-BACK UNIT", 10, 10},
    {"head", "frame", "HEAD", 0, 0},
    {"core", "frame", "CORE", 1, 1},
    {"arms", "frame", "ARMS", 2, 2},
    {"legs", "frame", "LEGS", 3, 3},
    {"booster", "inner", "BOOSTER", 4, 4},
    {"fcs", "inner", "FCS", 6, 5},
    {"generator", "inner", "GENERATOR", 5, 6},
    {"expansion", "expansion", "EXPANSION", 15, 15},
}};

QJsonDocument readJsonResource(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("failed to open Advanced Garage resource: " + path.toStdString());
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (document.isNull()) {
        throw std::runtime_error("failed to parse Advanced Garage resource: " + path.toStdString());
    }
    return document;
}

std::vector<QString> readTextResourceLines(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("failed to open Advanced Garage text resource: " + path.toStdString());
    }

    QTextStream stream(&file);
    std::vector<QString> lines;
    while (!stream.atEnd()) {
        const auto line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::string trimAscii(std::string value) {
    const auto isSpace = [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool endsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

std::string normalizeAdvancedGaragePartName(std::string value) {
    value = trimAscii(std::move(value));
    const auto dashPos = value.rfind(" - ");
    if (dashPos != std::string::npos) {
        value = value.substr(dashPos + 3);
        value = trimAscii(std::move(value));
    }

    static constexpr std::array<std::string_view, 9> kKnownSuffixes{{
        " (Right)",
        " (Left)",
        " (Damaged)",
        " (Escape Mission)",
        " (Escape Mission Variant)",
        " (Escape)",
        " [Inherited]",
        " [Inherited from Escape Mission]",
        " [NPC]",
    }};

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto suffix : kKnownSuffixes) {
            if (endsWith(value, suffix)) {
                value.resize(value.size() - suffix.size());
                value = trimAscii(std::move(value));
                changed = true;
            }
        }
    }

    return value;
}

std::optional<std::string> slotKeyForProtectorEntry(const std::string& lineRemainder) {
    const auto dashPos = lineRemainder.find(" - ");
    const auto prefix = dashPos == std::string::npos
        ? lineRemainder
        : lineRemainder.substr(0, dashPos);
    if (prefix.rfind("HEAD", 0) == 0) {
        return std::string("head");
    }
    if (prefix.rfind("CORE", 0) == 0) {
        return std::string("core");
    }
    if (prefix.rfind("ARMS", 0) == 0) {
        return std::string("arms");
    }
    if (prefix.rfind("LEGS", 0) == 0) {
        return std::string("legs");
    }
    return std::nullopt;
}

std::optional<std::string> slotKeyForWeaponRowId(const std::uint32_t rowId) {
    if (rowId >= 10000000U && rowId < 15000000U) {
        return std::string("rightArm");
    }
    if (rowId >= 15000000U && rowId < 25000000U) {
        return std::string("leftArm");
    }
    if (rowId >= 25000000U && rowId < 30000000U) {
        return std::string("leftBack");
    }
    if (rowId >= 30000000U && rowId < 35000000U) {
        return std::string("rightBack");
    }
    if (rowId >= 35000000U && rowId < 40000000U) {
        return std::string("leftBack");
    }

    switch (rowId) {
    case 95020000U:
        return std::string("rightArm");
    case 95020100U:
        return std::string("leftArm");
    case 95020200U:
        return std::string("rightBack");
    case 95020300U:
        return std::string("leftBack");
    default:
        return std::nullopt;
    }
}

void appendNamedRowMapping(
    PreviewLookup& lookup,
    const std::string& slotKey,
    const std::uint32_t rowId,
    const std::string& rawPartName) {
    const auto normalizedName = normalizeAdvancedGaragePartName(rawPartName);
    if (normalizedName.empty()) {
        return;
    }

    const auto partIt = lookup.normalizedNameToPartId.find(normalizedName);
    if (partIt == lookup.normalizedNameToPartId.end()) {
        return;
    }

    lookup.namedRowToPartId[slotKey].emplace(rowId, partIt->second);
}

void loadNamedWeaponRows(PreviewLookup& lookup) {
    for (const auto& line : readTextResourceLines(QStringLiteral(":/advanced_garage/names/EquipParamWeapon.txt"))) {
        const auto separator = line.indexOf(' ');
        if (separator <= 0) {
            continue;
        }

        bool ok = false;
        const auto rowId = static_cast<std::uint32_t>(line.left(separator).toULongLong(&ok));
        if (!ok) {
            continue;
        }

        const auto slotKey = slotKeyForWeaponRowId(rowId);
        if (!slotKey.has_value()) {
            continue;
        }
        appendNamedRowMapping(lookup, *slotKey, rowId, line.mid(separator + 1).toStdString());
    }
}

void loadNamedProtectorRows(PreviewLookup& lookup) {
    for (const auto& line : readTextResourceLines(QStringLiteral(":/advanced_garage/names/EquipParamProtector.txt"))) {
        const auto separator = line.indexOf(' ');
        if (separator <= 0) {
            continue;
        }

        bool ok = false;
        const auto rowId = static_cast<std::uint32_t>(line.left(separator).toULongLong(&ok));
        if (!ok) {
            continue;
        }

        const auto remainder = line.mid(separator + 1).toStdString();
        const auto slotKey = slotKeyForProtectorEntry(remainder);
        if (!slotKey.has_value()) {
            continue;
        }
        appendNamedRowMapping(lookup, *slotKey, rowId, remainder);
    }
}

void loadNamedSimpleRows(PreviewLookup& lookup, const QString& path, const std::string& slotKey) {
    for (const auto& line : readTextResourceLines(path)) {
        const auto separator = line.indexOf(' ');
        if (separator <= 0) {
            continue;
        }

        bool ok = false;
        const auto rowId = static_cast<std::uint32_t>(line.left(separator).toULongLong(&ok));
        if (!ok) {
            continue;
        }
        appendNamedRowMapping(lookup, slotKey, rowId, line.mid(separator + 1).toStdString());
    }
}

PreviewLookup loadPreviewLookup() {
    PreviewLookup lookup;

    const auto partsDoc = readJsonResource(QStringLiteral(":/advanced_garage/data/PartsData.json"));
    if (!partsDoc.isArray()) {
        throw std::runtime_error("Advanced Garage PartsData.json must be a JSON array");
    }

    int nextId = 0;
    auto appendPart = [&](const QJsonObject& object) {
        AdvancedGaragePartEntry part;
        part.id = nextId++;
        part.name = object.value(QStringLiteral("Name")).toString().toStdString();
        part.kind = object.value(QStringLiteral("Kind")).toString().toStdString();
        part.manufacturer = object.value(QStringLiteral("Manufacturer")).toString().toStdString();
        lookup.normalizedNameToPartId.emplace(normalizeAdvancedGaragePartName(part.name), part.id);
        lookup.partsById.emplace(part.id, std::move(part));
        return nextId - 1;
    };

    for (const auto& value : partsDoc.array()) {
        appendPart(value.toObject());
    }
    lookup.unitNothingPartId = appendPart(QJsonObject{
        {QStringLiteral("Name"), QStringLiteral("(NOTHING)")},
        {QStringLiteral("Kind"), QStringLiteral("Unit")},
        {QStringLiteral("Manufacturer"), QStringLiteral("")},
    });
    lookup.boosterNothingPartId = appendPart(QJsonObject{
        {QStringLiteral("Name"), QStringLiteral("(NOTHING)")},
        {QStringLiteral("Kind"), QStringLiteral("Booster")},
        {QStringLiteral("Manufacturer"), QStringLiteral("")},
    });
    lookup.expansionNothingPartId = appendPart(QJsonObject{
        {QStringLiteral("Name"), QStringLiteral("(NOTHING)")},
        {QStringLiteral("Kind"), QStringLiteral("Expansion")},
        {QStringLiteral("Manufacturer"), QStringLiteral("")},
    });

    loadNamedWeaponRows(lookup);
    loadNamedProtectorRows(lookup);
    loadNamedSimpleRows(lookup, QStringLiteral(":/advanced_garage/names/EquipParamBooster.txt"), "booster");
    loadNamedSimpleRows(lookup, QStringLiteral(":/advanced_garage/names/EquipParamFcs.txt"), "fcs");
    loadNamedSimpleRows(lookup, QStringLiteral(":/advanced_garage/names/EquipParamGenerator.txt"), "generator");

    const auto lookupDoc = readJsonResource(QStringLiteral(":/advanced_garage/data/ac_preview_lookup.json"));
    const auto root = lookupDoc.object();

    const auto slotWordsObject = root.value(QStringLiteral("slotWords")).toObject();
    for (auto it = slotWordsObject.begin(); it != slotWordsObject.end(); ++it) {
        lookup.legacySlotWords.emplace(it.key().toStdString(), it.value().toInt(-1));
    }
    lookup.noneSentinel = static_cast<std::uint32_t>(
        root.value(QStringLiteral("noneSentinel")).toVariant().toULongLong());

    const auto slotLookupObject = root.value(QStringLiteral("slotRowToAgPartId")).toObject();
    for (auto slotIt = slotLookupObject.begin(); slotIt != slotLookupObject.end(); ++slotIt) {
        auto& slotMap = lookup.legacyRawSlotToPartId[slotIt.key().toStdString()];
        const auto rowObject = slotIt.value().toObject();
        for (auto rowIt = rowObject.begin(); rowIt != rowObject.end(); ++rowIt) {
            const auto rowId = static_cast<std::uint32_t>(rowIt.key().toUInt());
            const auto partId = rowIt.value().toInt(-1);
            slotMap.emplace(rowId, partId);

            if (lookup.ambiguousLegacyGlobalRows.find(rowId) != lookup.ambiguousLegacyGlobalRows.end()) {
                continue;
            }
            const auto [globalIt, inserted] = lookup.legacyGlobalRowToPartId.emplace(rowId, partId);
            if (!inserted && globalIt->second != partId) {
                lookup.legacyGlobalRowToPartId.erase(globalIt);
                lookup.ambiguousLegacyGlobalRows.insert(rowId);
            }
        }
    }

    const auto provisionalSlotLookupObject = root.value(QStringLiteral("provisionalSlotRowToAgPartId")).toObject();
    for (auto slotIt = provisionalSlotLookupObject.begin(); slotIt != provisionalSlotLookupObject.end(); ++slotIt) {
        auto& slotMap = lookup.provisionalLegacyRawSlotToPartId[slotIt.key().toStdString()];
        const auto rowObject = slotIt.value().toObject();
        for (auto rowIt = rowObject.begin(); rowIt != rowObject.end(); ++rowIt) {
            const auto rowId = static_cast<std::uint32_t>(rowIt.key().toUInt());
            const auto partId = rowIt.value().toInt(-1);
            slotMap.emplace(rowId, partId);
        }
    }

    return lookup;
}

const PreviewLookup& previewLookup() {
    static const PreviewLookup lookup = loadPreviewLookup();
    return lookup;
}

contracts::AcAssemblySlotDto makeSlotDto(
    const std::string& slotKey,
    const std::string& groupKey,
    const std::string& slotLabel) {
    return contracts::AcAssemblySlotDto{
        .slotKey = slotKey,
        .groupKey = groupKey,
        .slotLabel = slotLabel,
        .partName = "(UNRESOLVED)",
        .manufacturer = {},
        .advancedGaragePartId = std::nullopt,
        .hasMatch = false,
    };
}

std::array<contracts::AcAssemblySlotDto, 12> buildSlotSkeleton() {
    return {
        makeSlotDto("rightArm", "unit", "R-ARM UNIT"),
        makeSlotDto("leftArm", "unit", "L-ARM UNIT"),
        makeSlotDto("rightBack", "unit", "R-BACK UNIT"),
        makeSlotDto("leftBack", "unit", "L-BACK UNIT"),
        makeSlotDto("head", "frame", "HEAD"),
        makeSlotDto("core", "frame", "CORE"),
        makeSlotDto("arms", "frame", "ARMS"),
        makeSlotDto("legs", "frame", "LEGS"),
        makeSlotDto("booster", "inner", "BOOSTER"),
        makeSlotDto("fcs", "inner", "FCS"),
        makeSlotDto("generator", "inner", "GENERATOR"),
        makeSlotDto("expansion", "expansion", "EXPANSION"),
    };
}

int slotIndexForKey(const std::string& slotKey) {
    static const std::unordered_map<std::string, int> kSlotIndices{
        {"rightArm", 0},
        {"leftArm", 1},
        {"rightBack", 2},
        {"leftBack", 3},
        {"head", 4},
        {"core", 5},
        {"arms", 6},
        {"legs", 7},
        {"booster", 8},
        {"fcs", 9},
        {"generator", 10},
        {"expansion", 11},
    };
    const auto it = kSlotIndices.find(slotKey);
    return it == kSlotIndices.end() ? -1 : it->second;
}

bool isUnitSlot(const std::string& slotKey) {
    return slotKey == "rightArm"
        || slotKey == "leftArm"
        || slotKey == "rightBack"
        || slotKey == "leftBack";
}

std::optional<int> specialNothingPartIdForSlot(const PreviewLookup& lookup, const std::string& slotKey) {
    if (slotKey == "booster") {
        return lookup.boosterNothingPartId >= 0 ? std::optional<int>(lookup.boosterNothingPartId) : std::nullopt;
    }
    if (slotKey == "expansion") {
        return lookup.expansionNothingPartId >= 0 ? std::optional<int>(lookup.expansionNothingPartId) : std::nullopt;
    }
    if (isUnitSlot(slotKey)) {
        return lookup.unitNothingPartId >= 0 ? std::optional<int>(lookup.unitNothingPartId) : std::nullopt;
    }
    return std::nullopt;
}

void applyResolvedPart(
    contracts::AcPreviewDto& preview,
    const int slotIndex,
    const int partId) {
    const auto& lookup = previewLookup();
    const auto partIt = lookup.partsById.find(partId);
    if (partIt == lookup.partsById.end()) {
        return;
    }

    auto& slotEntry = preview.assemblySlots[static_cast<std::size_t>(slotIndex)];
    slotEntry.partName = partIt->second.name;
    slotEntry.manufacturer = partIt->second.manufacturer;
    slotEntry.advancedGaragePartId = partIt->second.id;
    slotEntry.hasMatch = true;
    preview.compatibleBuildIds[static_cast<std::size_t>(slotIndex)] = partIt->second.id;
}

std::optional<std::uint32_t> transformRawValueToNamedRowId(
    const std::string& slotKey,
    const std::uint32_t rawValue) {
    if (slotKey == "head" || slotKey == "core" || slotKey == "arms" || slotKey == "legs") {
        return rawValue >= 0x10000000U ? std::optional<std::uint32_t>(rawValue - 0x10000000U) : std::nullopt;
    }
    if (slotKey == "booster") {
        return rawValue >= 0x60000000U ? std::optional<std::uint32_t>(rawValue - 0x60000000U) : std::nullopt;
    }
    if (slotKey == "fcs") {
        return rawValue >= 0x70000000U ? std::optional<std::uint32_t>(rawValue - 0x70000000U) : std::nullopt;
    }
    if (slotKey == "generator") {
        return rawValue >= 0x50000000U ? std::optional<std::uint32_t>(rawValue - 0x50000000U) : std::nullopt;
    }
    return rawValue;
}

std::optional<int> resolveViaNamedRows(
    const PreviewLookup& lookup,
    const std::string& slotKey,
    const std::uint32_t rawValue) {
    const auto slotMapIt = lookup.namedRowToPartId.find(slotKey);
    if (slotMapIt == lookup.namedRowToPartId.end()) {
        return std::nullopt;
    }

    const auto transformedRowId = transformRawValueToNamedRowId(slotKey, rawValue);
    if (!transformedRowId.has_value()) {
        return std::nullopt;
    }

    const auto rowIt = slotMapIt->second.find(*transformedRowId);
    if (rowIt == slotMapIt->second.end()) {
        return std::nullopt;
    }
    return rowIt->second;
}

std::optional<LegacyResolution> resolveViaLegacyRawLookup(
    const PreviewLookup& lookup,
    const std::string& slotKey,
    const std::uint32_t rawValue) {
    const auto slotMapIt = lookup.legacyRawSlotToPartId.find(slotKey);
    if (slotMapIt != lookup.legacyRawSlotToPartId.end()) {
        const auto rowIt = slotMapIt->second.find(rawValue);
        if (rowIt != slotMapIt->second.end()) {
            return LegacyResolution{.partId = rowIt->second, .provisional = false};
        }
    }

    const auto provisionalSlotMapIt = lookup.provisionalLegacyRawSlotToPartId.find(slotKey);
    if (provisionalSlotMapIt != lookup.provisionalLegacyRawSlotToPartId.end()) {
        const auto rowIt = provisionalSlotMapIt->second.find(rawValue);
        if (rowIt != provisionalSlotMapIt->second.end()) {
            return LegacyResolution{.partId = rowIt->second, .provisional = true};
        }
    }

    const auto globalIt = lookup.legacyGlobalRowToPartId.find(rawValue);
    if (globalIt != lookup.legacyGlobalRowToPartId.end()) {
        return LegacyResolution{.partId = globalIt->second, .provisional = false};
    }

    return std::nullopt;
}

bool resolveSlot(
    contracts::AcPreviewDto& preview,
    const PreviewSlotSpec& slotSpec,
    const std::array<std::uint32_t, 16>& assembleWords) {
    const auto slotIndex = slotIndexForKey(slotSpec.slotKey);
    if (slotIndex < 0
        || slotSpec.primaryWordIndex < 0
        || slotSpec.primaryWordIndex >= static_cast<int>(assembleWords.size())) {
        return false;
    }

    const auto& lookup = previewLookup();
    const auto tryApply = [&](const std::uint32_t rawValue) -> std::optional<bool> {
        if (rawValue == lookup.noneSentinel) {
            if (const auto nothingPartId = specialNothingPartIdForSlot(lookup, slotSpec.slotKey);
                nothingPartId.has_value()) {
                applyResolvedPart(preview, slotIndex, *nothingPartId);
            }
            return false;
        }

        if (const auto namedPartId = resolveViaNamedRows(lookup, slotSpec.slotKey, rawValue);
            namedPartId.has_value()) {
            applyResolvedPart(preview, slotIndex, *namedPartId);
            return false;
        }

        if (const auto legacy = resolveViaLegacyRawLookup(lookup, slotSpec.slotKey, rawValue);
            legacy.has_value()) {
            applyResolvedPart(preview, slotIndex, legacy->partId);
            return legacy->provisional;
        }

        return std::nullopt;
    };

    if (const auto resolved = tryApply(assembleWords[static_cast<std::size_t>(slotSpec.primaryWordIndex)]);
        resolved.has_value()) {
        return *resolved;
    }

    if (slotSpec.legacyFallbackWordIndex >= 0
        && slotSpec.legacyFallbackWordIndex < static_cast<int>(assembleWords.size())
        && slotSpec.legacyFallbackWordIndex != slotSpec.primaryWordIndex) {
        if (const auto resolved = tryApply(assembleWords[static_cast<std::size_t>(slotSpec.legacyFallbackWordIndex)]);
            resolved.has_value()) {
            return *resolved;
        }
    }

    return false;
}

}  // namespace

std::optional<contracts::AcPreviewDto> tryBuildAdvancedGaragePreview(
    const std::array<std::uint32_t, 16>& assembleWords) {
    contracts::AcPreviewDto preview;
    preview.assemblySlots = buildSlotSkeleton();
    preview.compatibleBuildIds.fill(std::nullopt);
    preview.note = "Preview mapping is still incomplete for this AC.";

    bool usedProvisionalAlias = false;
    for (const auto& slotSpec : kPreviewSlotSpecs) {
        usedProvisionalAlias = resolveSlot(preview, slotSpec, assembleWords) || usedProvisionalAlias;
    }

    bool allMatched = true;
    for (const auto& value : preview.compatibleBuildIds) {
        if (!value.has_value()) {
            allMatched = false;
            break;
        }
    }
    preview.buildLinkCompatible = allMatched && !usedProvisionalAlias;
    if (allMatched && !usedProvisionalAlias) {
        preview.note = "Advanced Garage preview mapping verified for all 12 slots.";
    } else if (allMatched && usedProvisionalAlias) {
        preview.note =
            "Preview resolves all 12 slots, but some legacy aliases are still provisional. "
            "Build-link export stays disabled until these aliases are verified.";
    }
    preview.buildLinkUrl = buildAdvancedGarageBuildLink(preview.compatibleBuildIds);

    return preview;
}

std::string buildAdvancedGarageBuildLink(const std::array<std::optional<int>, 12>& compatibleBuildIds) {
    for (const auto& id : compatibleBuildIds) {
        if (!id.has_value()) {
            return {};
        }
    }

    std::ostringstream stream;
    stream << "https://matteosal.github.io/ac6-advanced-garage/?build=";
    for (std::size_t index = 0; index < compatibleBuildIds.size(); ++index) {
        if (index > 0) {
            stream << '-';
        }
        stream << *compatibleBuildIds[index];
    }
    return stream.str();
}

std::string formatAssembleWordsForDetail(const std::array<std::uint32_t, 16>& assembleWords) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < assembleWords.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << "w" << std::setw(2) << std::setfill('0') << index
               << "=0x" << std::hex << std::setw(8) << std::setfill('0') << assembleWords[index]
               << std::dec;
    }
    return stream.str();
}

}  // namespace ac6dm::ac
