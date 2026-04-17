#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace ac6dm::workspace {

class SessionWorkspace final {
public:
    SessionWorkspace() = default;
    explicit SessionWorkspace(std::filesystem::path root);

    static SessionWorkspace create(
        const std::string& sessionId,
        const std::optional<std::filesystem::path>& preferredRuntimeRoot = std::nullopt);
    static std::filesystem::path chooseAsciiSafeRuntimeRoot();
    static bool isAsciiOnly(const std::filesystem::path& path);

    const std::filesystem::path& root() const noexcept;
    const std::filesystem::path& inputDir() const noexcept;
    const std::filesystem::path& unpackDir() const noexcept;
    const std::filesystem::path& repackDir() const noexcept;
    const std::filesystem::path& supportDir() const noexcept;

    std::filesystem::path stageSourceSave(const std::filesystem::path& sourceSave) const;

private:
    std::filesystem::path root_{};
    std::filesystem::path inputDir_{};
    std::filesystem::path unpackDir_{};
    std::filesystem::path repackDir_{};
    std::filesystem::path supportDir_{};
};

}  // namespace ac6dm::workspace
