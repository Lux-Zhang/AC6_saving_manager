#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::platform {

struct ProcessReport final {
    std::vector<std::string> command;
    int returnCode{-1};
    std::string stdoutText;
    std::string stderrText;
};

class ProcessRunner final {
public:
    ProcessReport run(
        const std::string& program,
        const std::vector<std::string>& arguments,
        const std::optional<std::filesystem::path>& workingDirectory,
        const std::map<std::string, std::string>& environment,
        int timeoutMs) const;
};

}  // namespace ac6dm::platform
