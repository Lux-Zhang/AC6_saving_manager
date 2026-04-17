#include "ThirdPartyManifest.hpp"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ac6dm::sidecar {

namespace {

std::uintmax_t jsonToUIntMax(const QJsonValue& value) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return static_cast<std::uintmax_t>(value.toInteger());
#else
    return static_cast<std::uintmax_t>(value.toDouble());
#endif
}

QJsonDocument loadJsonDocument(const std::filesystem::path& path) {
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open manifest: " + path.generic_string());
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (document.isNull() || !document.isObject()) {
        throw std::runtime_error("Manifest is not a JSON object: " + path.generic_string());
    }
    return document;
}

std::vector<std::string> listActualFiles(const std::filesystem::path& appRoot, const std::filesystem::path& root) {
    std::vector<std::string> files;
    if (!std::filesystem::exists(root)) {
        return files;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        files.push_back(std::filesystem::relative(entry.path(), appRoot).generic_string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace

ThirdPartyManifest ThirdPartyManifest::load(const std::filesystem::path& manifestPath) {
    const auto json = loadJsonDocument(manifestPath).object();

    ThirdPartyManifest manifest;
    manifest.schemaVersion = json.value("schema_version").toInt(1);
    manifest.name = json.value("name").toString().toStdString();
    manifest.sourceBaseline = json.value("source_baseline").toString().toStdString();
    manifest.versionLabel = json.value("version_label").toString().toStdString();
    manifest.entrypoint = json.value("entrypoint").toString().toStdString();
    manifest.required = json.value("required").toBool(true);
    manifest.preflightPolicy = json.value("preflight_policy").toString().toStdString();
    manifest.generatedAt = json.value("generated_at").toString().toStdString();

    for (const auto& itemValue : json.value("files").toArray()) {
        const auto item = itemValue.toObject();
        manifest.files.push_back(ThirdPartyManifestFile{
            .path = item.value("path").toString().toStdString(),
            .sha256 = item.value("sha256").toString().toStdString(),
            .required = item.value("required").toBool(true),
            .fileRole = item.value("file_role").toString().toStdString(),
            .sizeBytes = jsonToUIntMax(item.value("size_bytes")),
        });
    }
    return manifest;
}

std::string sha256File(const std::filesystem::path& filePath) {
    QFile file(QString::fromStdWString(filePath.wstring()));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open file for sha256: " + filePath.generic_string());
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(1024 * 1024));
    }
    return hash.result().toHex().toStdString();
}

ManifestVerificationResult verifyManifest(
    const std::filesystem::path& appRoot,
    const ThirdPartyManifest& manifest,
    const std::filesystem::path& manifestPath,
    const std::optional<std::string>& expectedVersionLabel) {
    ManifestVerificationResult result;
    result.manifestPath = manifestPath;
    result.entrypointPath = appRoot / manifest.entrypoint;
    result.actualVersionLabel = manifest.versionLabel;
    result.expectedVersionLabel = expectedVersionLabel;

    std::filesystem::path toolRoot = appRoot / "third_party" / "WitchyBND";
    std::vector<std::string> actualFiles = listActualFiles(appRoot, toolRoot);

    std::vector<std::string> expectedPaths;
    expectedPaths.reserve(manifest.files.size());
    for (const auto& file : manifest.files) {
        expectedPaths.push_back(file.path);
        if (file.required && !std::filesystem::exists(appRoot / file.path)) {
            result.missingFiles.push_back(file.path);
        }
    }
    std::sort(expectedPaths.begin(), expectedPaths.end());

    for (const auto& actual : actualFiles) {
        if (!std::binary_search(expectedPaths.begin(), expectedPaths.end(), actual)) {
            result.extraFiles.push_back(actual);
        }
    }

    for (const auto& file : manifest.files) {
        const auto candidate = appRoot / file.path;
        if (!std::filesystem::exists(candidate) || !std::filesystem::is_regular_file(candidate)) {
            continue;
        }
        const auto actualHash = sha256File(candidate);
        if (actualHash != file.sha256) {
            result.hashMismatches.push_back(file.path);
        }
    }

    if (!std::filesystem::exists(result.entrypointPath)) {
        result.errors.push_back("entrypoint missing: " + manifest.entrypoint);
    }
    if (!result.missingFiles.empty()) {
        result.errors.push_back("required files missing");
    }
    if (!result.extraFiles.empty()) {
        result.errors.push_back("unexpected extra files present");
    }
    if (!result.hashMismatches.empty()) {
        result.errors.push_back("sha256 mismatch detected");
    }
    if (expectedVersionLabel.has_value() && *expectedVersionLabel != manifest.versionLabel) {
        result.errors.push_back("version mismatch");
    }

    result.success = result.errors.empty();
    return result;
}

}  // namespace ac6dm::sidecar
