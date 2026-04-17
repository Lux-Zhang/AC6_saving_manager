#include "SessionWorkspace.hpp"

#include <QDir>
#include <QStandardPaths>

#include <cstdlib>
#include <stdexcept>

namespace ac6dm::workspace {

namespace {

std::string sanitizeSessionId(std::string sessionId) {
    for (auto& ch : sessionId) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        if (!ok) {
            ch = '_';
        }
    }
    if (sessionId.empty()) {
        sessionId = "default-session";
    }
    return sessionId;
}

std::filesystem::path makePreferredRoot(const char* envName) {
    if (const char* raw = std::getenv(envName); raw != nullptr && *raw != '\0') {
        return std::filesystem::path(raw) / "AC6DM" / "runtime";
    }
    return {};
}

}  // namespace

SessionWorkspace::SessionWorkspace(std::filesystem::path root)
    : root_(std::move(root)),
      inputDir_(root_ / "input"),
      unpackDir_(root_ / "unpacked"),
      repackDir_(root_ / "repacked"),
      supportDir_(root_ / "support") {}

SessionWorkspace SessionWorkspace::create(
    const std::string& sessionId,
    const std::optional<std::filesystem::path>& preferredRuntimeRoot) {
    std::filesystem::path runtimeBase;
    if (preferredRuntimeRoot.has_value() && isAsciiOnly(*preferredRuntimeRoot)) {
        runtimeBase = *preferredRuntimeRoot;
        std::filesystem::create_directories(runtimeBase);
    } else {
        runtimeBase = chooseAsciiSafeRuntimeRoot();
    }

    const auto runtimeRoot = runtimeBase / sanitizeSessionId(sessionId);
    std::filesystem::create_directories(runtimeRoot / "input");
    std::filesystem::create_directories(runtimeRoot / "unpacked");
    std::filesystem::create_directories(runtimeRoot / "repacked");
    std::filesystem::create_directories(runtimeRoot / "support");
    return SessionWorkspace(runtimeRoot);
}

std::filesystem::path SessionWorkspace::chooseAsciiSafeRuntimeRoot() {
    const std::vector<std::filesystem::path> candidates = {
        makePreferredRoot("LOCALAPPDATA"),
        makePreferredRoot("TEMP"),
        makePreferredRoot("TMP"),
#ifdef _WIN32
        std::filesystem::path("C:/AC6DM/runtime"),
#else
        std::filesystem::path("/tmp/AC6DM/runtime"),
#endif
    };

    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        if (!isAsciiOnly(candidate)) {
            continue;
        }
        std::filesystem::create_directories(candidate);
        return candidate;
    }

    const auto fallback = std::filesystem::temp_directory_path() / "AC6DM" / "runtime";
    std::filesystem::create_directories(fallback);
    return fallback;
}

bool SessionWorkspace::isAsciiOnly(const std::filesystem::path& path) {
    const auto raw = path.generic_string();
    for (const unsigned char ch : raw) {
        if (ch > 0x7F) {
            return false;
        }
    }
    return true;
}

const std::filesystem::path& SessionWorkspace::root() const noexcept { return root_; }
const std::filesystem::path& SessionWorkspace::inputDir() const noexcept { return inputDir_; }
const std::filesystem::path& SessionWorkspace::unpackDir() const noexcept { return unpackDir_; }
const std::filesystem::path& SessionWorkspace::repackDir() const noexcept { return repackDir_; }
const std::filesystem::path& SessionWorkspace::supportDir() const noexcept { return supportDir_; }

std::filesystem::path SessionWorkspace::stageSourceSave(const std::filesystem::path& sourceSave) const {
    if (!std::filesystem::exists(sourceSave) || !std::filesystem::is_regular_file(sourceSave)) {
        throw std::runtime_error("Source save does not exist: " + sourceSave.generic_string());
    }
    const auto destination = inputDir_ / sourceSave.filename();
    std::filesystem::copy_file(sourceSave, destination, std::filesystem::copy_options::overwrite_existing);
    return destination;
}

}  // namespace ac6dm::workspace
