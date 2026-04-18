#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ac6dm::ac {

struct RegulationRuntimeMetadata final {
    std::string validatedUserData010PlainSha256;
    std::uint64_t validatedUserData010PlainLength{0};
    std::uint32_t validatedUserData010EmbeddedOffset{20};
    std::string validatedUserData010HeaderMagic;
    std::uint32_t validatedUserData010Version{0};
};

struct RegulationCharaEquipRow final {
    int charaId{-1};
    std::string name;
    std::map<std::string, int> equipFields;
};

struct RegulationArenaLinkRow final {
    int arenaId{-1};
    std::string name;
    int accountId{-1};
    int charaId{-1};
    int npcId{-1};
    int thinkId{-1};
};

struct RegulationPresetCatalog final {
    std::string generatedAt;
    std::string source;
    RegulationRuntimeMetadata runtimeMetadata;
    std::vector<RegulationCharaEquipRow> charaRows;
    std::vector<RegulationArenaLinkRow> arenaRows;
};

const RegulationPresetCatalog& regulationPresetCatalog();

const RegulationCharaEquipRow* findRegulationCharaRow(int charaId);
const RegulationArenaLinkRow* findRegulationArenaRow(int arenaId);
const RegulationCharaEquipRow* findRegulationArenaResolvedCharaRow(int arenaId);
std::optional<int> findRegulationCharaEquipValue(
    const RegulationCharaEquipRow& row,
    std::string_view fieldName);

}  // namespace ac6dm::ac
