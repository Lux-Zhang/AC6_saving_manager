#pragma once

#include <cstdio>
#include <optional>
#include <string>

namespace ac6dm::contracts {

inline std::string formatUserSlotLabel(const std::optional<int>& slotId) {
    if (!slotId.has_value()) {
        return "SHARE";
    }
    char buffer[32] = {0};
    std::snprintf(buffer, sizeof(buffer), "USER_EMBLEM_%02d", *slotId);
    return std::string(buffer);
}

}  // namespace ac6dm::contracts
