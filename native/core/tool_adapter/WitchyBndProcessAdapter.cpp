#include "WitchyBndProcessAdapter.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace ac6dm::tool_adapter {

namespace {

constexpr char kUnpackMetadataFilename[] = ".ac6dm_unpack_meta.txt";

std::filesystem::path manifestPathFor(const std::filesystem::path& appRoot) {
    return appRoot / "third_party" / "third_party_manifest.json";
}

std::filesystem::path defaultUnpackDirectory(const std::filesystem::path& containerPath) {
    const auto suffix = containerPath.extension().string();
    const auto cleanSuffix = suffix.empty() ? "unpacked" : suffix.substr(1);
    auto baseName = containerPath.stem().string();
    std::replace(baseName.begin(), baseName.end(), '.', '-');
    return containerPath.parent_path() / (baseName + "-" + cleanSuffix);
}

std::filesystem::path relocateOutput(
    const std::filesystem::path& actualPath,
    const std::optional<std::filesystem::path>& requestedPath) {
    if (!requestedPath.has_value()) {
        return actualPath;
    }

    const auto& target = *requestedPath;
    if (std::filesystem::exists(target) && std::filesystem::equivalent(actualPath, target)) {
        return actualPath;
    }

    std::filesystem::create_directories(target.parent_path());
    if (std::filesystem::exists(target)) {
        if (std::filesystem::is_directory(target)) {
            std::filesystem::remove_all(target);
        } else {
            std::filesystem::remove(target);
        }
    }
    std::filesystem::rename(actualPath, target);
    return target;
}

void writeUnpackMetadata(
    const std::filesystem::path& unpackedDirectory,
    const std::filesystem::path& containerPath) {
    std::ofstream output(unpackedDirectory / kUnpackMetadataFilename, std::ios::binary | std::ios::trunc);
    output << containerPath.filename().generic_string() << "\n";
}

std::string readContainerName(const std::filesystem::path& unpackedDirectory) {
    std::ifstream input(unpackedDirectory / kUnpackMetadataFilename, std::ios::binary);
    std::string containerName;
    std::getline(input, containerName);
    if (containerName.empty()) {
        throw std::runtime_error(
            "Unpacked directory metadata is missing container name: "
            + (unpackedDirectory / kUnpackMetadataFilename).generic_string());
    }
    return containerName;
}

void removeBackupIfExists(const std::filesystem::path& containerPath) {
    auto backup = containerPath;
    backup += ".bak";
    if (std::filesystem::exists(backup) && std::filesystem::is_regular_file(backup)) {
        std::filesystem::remove(backup);
    }
}

std::string buildCommandSummary(
    const std::filesystem::path& entrypoint,
    const std::vector<std::wstring>& arguments) {
    std::string summary = entrypoint.generic_string();
    for (const auto& argument : arguments) {
        summary += " ";
        summary += std::filesystem::path(argument).generic_string();
    }
    return summary;
}

#ifdef _WIN32

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const auto requiredSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredSize <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(requiredSize), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        requiredSize,
        nullptr,
        nullptr);
    return result;
}

std::string formatWindowsError(const DWORD errorCode) {
    wchar_t* buffer = nullptr;
    const auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto length = FormatMessageW(
        flags,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (length > 0 && buffer != nullptr) {
        message.assign(buffer, static_cast<std::size_t>(length));
        LocalFree(buffer);
    }

    while (!message.empty()) {
        const auto tail = message.back();
        if (tail == L'\r' || tail == L'\n' || tail == L' ' || tail == L'\t') {
            message.pop_back();
            continue;
        }
        break;
    }

    auto text = std::string("Windows error ") + std::to_string(errorCode);
    if (!message.empty()) {
        text += ": ";
        text += wideToUtf8(message);
    }
    return text;
}

std::wstring quoteWindowsArgument(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }

    if (value.find_first_of(L" \t\"") == std::wstring::npos) {
        return value;
    }

    std::wstring quoted;
    quoted.push_back(L'"');

    std::size_t pendingBackslashes = 0;
    for (const auto ch : value) {
        if (ch == L'\\') {
            ++pendingBackslashes;
            continue;
        }

        if (ch == L'"') {
            quoted.append(pendingBackslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            pendingBackslashes = 0;
            continue;
        }

        if (pendingBackslashes > 0) {
            quoted.append(pendingBackslashes, L'\\');
            pendingBackslashes = 0;
        }

        quoted.push_back(ch);
    }

    if (pendingBackslashes > 0) {
        quoted.append(pendingBackslashes * 2, L'\\');
    }

    quoted.push_back(L'"');
    return quoted;
}

std::wstring buildCommandLine(
    const std::filesystem::path& entrypoint,
    const std::vector<std::wstring>& arguments) {
    std::wstring commandLine = quoteWindowsArgument(entrypoint.wstring());
    for (const auto& argument : arguments) {
        commandLine.push_back(L' ');
        commandLine += quoteWindowsArgument(argument);
    }
    return commandLine;
}

platform::ProcessReport runConsoleBackedProcess(
    const std::filesystem::path& entrypoint,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& workingDirectory,
    int timeoutMs) {
    platform::ProcessReport report;
    report.command.push_back(entrypoint.generic_string());
    for (const auto& argument : arguments) {
        report.command.push_back(std::filesystem::path(argument).generic_string());
    }

    auto commandLine = buildCommandLine(entrypoint, arguments);
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWMINNOACTIVE;

    PROCESS_INFORMATION processInfo{};
    const auto created = CreateProcessW(
        entrypoint.wstring().c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        workingDirectory.wstring().c_str(),
        &startupInfo,
        &processInfo);
    if (!created) {
        report.returnCode = 1;
        report.stderrText = formatWindowsError(GetLastError());
        return report;
    }

    const auto waitResult = WaitForSingleObject(processInfo.hProcess, static_cast<DWORD>(timeoutMs));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(processInfo.hProcess, 1);
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        report.returnCode = 1;
        report.stderrText = "process timed out";
    } else if (waitResult != WAIT_OBJECT_0) {
        report.returnCode = 1;
        report.stderrText = formatWindowsError(GetLastError());
    } else {
        DWORD exitCode = 1;
        if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
            report.returnCode = 1;
            report.stderrText = formatWindowsError(GetLastError());
        } else {
            report.returnCode = static_cast<int>(exitCode);
        }
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    if (report.returnCode != 0 && report.stderrText.empty()) {
        report.stderrText =
            "sidecar process exited via console-backed launcher; stdout/stderr are not captured in GUI mode";
    }
    return report;
}

#else

platform::ProcessReport runConsoleBackedProcess(
    const std::filesystem::path& entrypoint,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& workingDirectory,
    int timeoutMs) {
    platform::ProcessReport report;
    report.command.push_back(buildCommandSummary(entrypoint, arguments));
    report.returnCode = 1;
    report.stderrText =
        "console-backed WitchyBND launcher is only implemented for Windows in the native desktop build";
    (void) workingDirectory;
    (void) timeoutMs;
    return report;
}

#endif

}  // namespace

WitchyBndProcessAdapter::WitchyBndProcessAdapter(std::filesystem::path appRoot, int timeoutMs)
    : appRoot_(std::move(appRoot)), timeoutMs_(timeoutMs) {}

sidecar::ManifestVerificationResult WitchyBndProcessAdapter::verifyBundledManifest(
    const std::optional<std::string>& expectedVersionLabel) const {
    const auto manifestPath = manifestPathFor(appRoot_);
    const auto manifest = sidecar::ThirdPartyManifest::load(manifestPath);
    return sidecar::verifyManifest(appRoot_, manifest, manifestPath, expectedVersionLabel);
}

std::filesystem::path WitchyBndProcessAdapter::resolveEntrypoint() const {
    const auto manifest = sidecar::ThirdPartyManifest::load(manifestPathFor(appRoot_));
    const auto entrypoint = appRoot_ / manifest.entrypoint;
    if (!std::filesystem::exists(entrypoint)) {
        throw std::runtime_error("Bundled WitchyBND entrypoint does not exist: " + entrypoint.generic_string());
    }
    return entrypoint;
}

platform::ProcessReport WitchyBndProcessAdapter::probeVersion() const {
    const auto entrypoint = resolveEntrypoint();
    return processRunner_.run(
        entrypoint.generic_string(),
        {"--version"},
        std::nullopt,
        {},
        timeoutMs_);
}

SidecarRunResult WitchyBndProcessAdapter::unpack(
    const std::filesystem::path& containerPath,
    const std::optional<std::filesystem::path>& expectedDirectory) const {
    const auto entrypoint = resolveEntrypoint();
    auto report =
        runConsoleBackedProcess(entrypoint, {L"-s", containerPath.wstring()}, containerPath.parent_path(), timeoutMs_);

    SidecarRunResult result;
    result.process = report;
    if (report.returnCode != 0) {
        return result;
    }

    const auto actualDirectory = defaultUnpackDirectory(containerPath);
    if (!std::filesystem::exists(actualDirectory) || !std::filesystem::is_directory(actualDirectory)) {
        result.process.returnCode = 1;
        result.process.stderrText = "WitchyBND did not create expected unpack directory: " + actualDirectory.generic_string();
        return result;
    }

    result.outputPath = relocateOutput(actualDirectory, expectedDirectory);
    writeUnpackMetadata(result.outputPath, containerPath);
    return result;
}

SidecarRunResult WitchyBndProcessAdapter::repack(
    const std::filesystem::path& unpackedDirectory,
    const std::optional<std::filesystem::path>& expectedContainer) const {
    const auto entrypoint = resolveEntrypoint();
    auto report = runConsoleBackedProcess(
        entrypoint,
        {L"-s", unpackedDirectory.wstring()},
        unpackedDirectory.parent_path(),
        timeoutMs_);

    SidecarRunResult result;
    result.process = report;
    if (report.returnCode != 0) {
        return result;
    }

    const auto actualContainer = unpackedDirectory.parent_path() / readContainerName(unpackedDirectory);
    if (!std::filesystem::exists(actualContainer) || !std::filesystem::is_regular_file(actualContainer)) {
        result.process.returnCode = 1;
        result.process.stderrText = "WitchyBND did not create expected repacked container: " + actualContainer.generic_string();
        return result;
    }

    removeBackupIfExists(actualContainer);
    result.outputPath = relocateOutput(actualContainer, expectedContainer);
    return result;
}

}  // namespace ac6dm::tool_adapter
