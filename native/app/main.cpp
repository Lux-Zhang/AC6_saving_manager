#include "app_services.hpp"
#include "product_gui_application.hpp"
#include "core/ac/ac_catalog.hpp"

#include <QByteArray>
#include <QFile>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace {

bool isConsoleMode(const int argc, char** argv) {
    if (argc < 2 || argv == nullptr || argv[1] == nullptr) {
        return false;
    }
    const auto mode = std::string_view(argv[1]);
    return mode == "--probe-open-save"
        || mode == "--probe-open-save-ac-debug"
        || mode == "--probe-open-save-ac-rows"
        || mode == "--probe-import-first-share"
        || mode == "--probe-import-first-share-ac";
}

void attachConsoleIfNeeded(const int argc, char** argv) {
#ifdef _WIN32
    if (!isConsoleMode(argc, argv)) {
        return;
    }

    ::AttachConsole(ATTACH_PARENT_PROCESS);

    auto rebindFile = [](const DWORD stdHandleId, FILE* stream, const char* mode) {
        const HANDLE handle = ::GetStdHandle(stdHandleId);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
            return;
        }
        const int crtHandle = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_TEXT);
        if (crtHandle < 0) {
            return;
        }
        FILE* file = _fdopen(crtHandle, mode);
        if (file == nullptr) {
            _close(crtHandle);
            return;
        }
        if (stream == stdout) {
            *stdout = *file;
            setvbuf(stdout, nullptr, _IONBF, 0);
            return;
        }
        if (stream == stderr) {
            *stderr = *file;
            setvbuf(stderr, nullptr, _IONBF, 0);
            return;
        }
        *stdin = *file;
    };

    rebindFile(STD_OUTPUT_HANDLE, stdout, "w");
    rebindFile(STD_ERROR_HANDLE, stderr, "w");
    rebindFile(STD_INPUT_HANDLE, stdin, "r");
    std::ios::sync_with_stdio(true);
#else
    (void)argc;
    (void)argv;
#endif
}

std::filesystem::path executableDirFromArgv(char** argv) {
    if (argv == nullptr || argv[0] == nullptr) {
        return std::filesystem::current_path();
    }
    const auto exePath = std::filesystem::path(QString::fromLocal8Bit(argv[0]).toStdWString());
    return exePath.has_parent_path() ? exePath.parent_path() : std::filesystem::current_path();
}

void ensureQtConf(const std::filesystem::path& appDir) {
    const auto qtConfPath = appDir / "qt.conf";
    if (std::filesystem::exists(qtConfPath)) {
        return;
    }
    QFile qtConf(QString::fromStdWString(qtConfPath.wstring()));
    if (!qtConf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    const QByteArray payload =
        "[Paths]\n"
        "Prefix = .\n"
        "Plugins = .\n"
        "Translations = translations\n";
    qtConf.write(payload);
}

void isolateQtRuntime(char** argv) {
    const auto appDir = executableDirFromArgv(argv);
    const auto platformsDir = appDir / "platforms";
    const auto stylesDir = appDir / "styles";
    const auto imageFormatsDir = appDir / "imageformats";
    const bool hasBundledQtPlugins = std::filesystem::exists(platformsDir);

    if (hasBundledQtPlugins) {
        ensureQtConf(appDir);
        qputenv("QT_PLUGIN_PATH", QByteArray());
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", QString::fromStdWString(platformsDir.wstring()).toLocal8Bit());
        qputenv("QT_QPA_GENERIC_PLUGINS", QByteArray());
        qputenv("QT_QPA_PLATFORMTHEME", QByteArray());
        qputenv("QT_STYLE_OVERRIDE", QByteArray());
        qputenv("QT_QPA_FONTDIR", QByteArray());
        qputenv("AC6DM_QT_APPDIR", QString::fromStdWString(appDir.wstring()).toLocal8Bit());
        qputenv("AC6DM_QT_STYLESDIR", QString::fromStdWString(stylesDir.wstring()).toLocal8Bit());
        qputenv("AC6DM_QT_IMAGEFORMATSDIR", QString::fromStdWString(imageFormatsDir.wstring()).toLocal8Bit());
    }

#ifdef _WIN32
    ::SetDllDirectoryW(appDir.c_str());
#endif
}

int runOpenSaveProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto savePath = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    auto services = ac6dm::app::AppServices::createDefault();
    const auto result = services.openSaveService->openSourceSave(savePath);

    std::cout << "summary=" << result.session.summary << "\n";
    std::cout << "workflowAvailable=" << (result.session.workflowAvailable ? "true" : "false") << "\n";
    std::cout << "hasRealSave=" << (result.session.hasRealSave ? "true" : "false") << "\n";
    std::cout << "catalogCount=" << result.catalog.size() << "\n";
    if (!result.catalog.empty()) {
        std::cout << "firstAsset=" << result.catalog.front().assetId << "\n";
    }
    int printedAcCount = 0;
    for (const auto& item : result.catalog) {
        if (item.assetKind != ac6dm::contracts::AssetKind::Ac) {
            continue;
        }
        std::cout << "acItem[" << printedAcCount << "].assetId=" << item.assetId << "\n";
        std::cout << "acItem[" << printedAcCount << "].archiveName=" << item.archiveName << "\n";
        std::cout << "acItem[" << printedAcCount << "].machineName=" << item.machineName << "\n";
        std::cout << "acItem[" << printedAcCount << "].shareCode=" << item.shareCode << "\n";
        if (++printedAcCount >= 12) {
            break;
        }
    }
    if (!result.diagnostics.empty()) {
        std::cout << "diagnosticCode=" << result.diagnostics.front().code << "\n";
        std::cout << "diagnosticMessage=" << result.diagnostics.front().message << "\n";
        for (const auto& detail : result.diagnostics.front().details) {
            std::cout << "detail=" << detail << "\n";
        }
    }
    return result.diagnostics.empty() ? 0 : 1;
}

int runOpenSaveFileProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto savePath = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    const auto reportPath = std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString());
    auto services = ac6dm::app::AppServices::createDefault();
    const auto result = services.openSaveService->openSourceSave(savePath);

    std::ofstream output(reportPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return 2;
    }

    output << "summary=" << result.session.summary << "\n";
    output << "workflowAvailable=" << (result.session.workflowAvailable ? "true" : "false") << "\n";
    output << "hasRealSave=" << (result.session.hasRealSave ? "true" : "false") << "\n";
    output << "catalogCount=" << result.catalog.size() << "\n";

    int printedAcCount = 0;
    for (const auto& item : result.catalog) {
        if (item.assetKind != ac6dm::contracts::AssetKind::Ac) {
            continue;
        }
        output << "acItem[" << printedAcCount << "].assetId=" << item.assetId << "\n";
        output << "acItem[" << printedAcCount << "].slotLabel=" << item.slotLabel << "\n";
        output << "acItem[" << printedAcCount << "].archiveName=" << item.archiveName << "\n";
        output << "acItem[" << printedAcCount << "].machineName=" << item.machineName << "\n";
        output << "acItem[" << printedAcCount << "].shareCode=" << item.shareCode << "\n";
        if (printedAcCount < 3) {
            for (const auto& field : item.detailFields) {
                output << "acItem[" << printedAcCount << "].detail." << field.label << "=" << field.value << "\n";
            }
        }
        if (++printedAcCount >= 50) {
            break;
        }
    }

    if (!result.diagnostics.empty()) {
        output << "diagnosticCode=" << result.diagnostics.front().code << "\n";
        output << "diagnosticMessage=" << result.diagnostics.front().message << "\n";
        for (const auto& detail : result.diagnostics.front().details) {
            output << "detail=" << detail << "\n";
        }
    }
    return result.diagnostics.empty() ? 0 : 1;
}

int runOpenSaveAcDebugProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto savePath = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    const auto reportPath = std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString());
    auto services = ac6dm::app::AppServices::createDefault();
    const auto result = services.openSaveService->openSourceSave(savePath);

    std::ofstream output(reportPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return 2;
    }

    output << "summary=" << result.session.summary << "\n";
    output << "workflowAvailable=" << (result.session.workflowAvailable ? "true" : "false") << "\n";
    output << "hasRealSave=" << (result.session.hasRealSave ? "true" : "false") << "\n";
    if (!result.session.artifactsRoot.has_value()) {
        output << "diagnostic=no-artifacts-root\n";
        return 1;
    }

    const auto unpackedDir = *result.session.artifactsRoot / "unpacked";
    output << "unpackedDir=" << unpackedDir.generic_string() << "\n";
    const auto summaries = ac6dm::ac::debugSummarizeContainers(unpackedDir);
    for (const auto& summary : summaries) {
        output << "container=" << summary.containerFile << "\n";
        output << "container.capacityField=" << summary.capacityField << "\n";
        output << "container.firstRecordDesignOffset=" << summary.firstRecordDesignOffset << "\n";
        output << "container.recordCountField=" << summary.recordCountField << "\n";
        output << "container.parsedRecordCount=" << summary.parsedRecordCount << "\n";
        output << "container.qualified=" << (summary.qualified ? "true" : "false") << "\n";
        output << "container.qualificationNote=" << summary.qualificationNote << "\n";
        output << "container.footerHex=" << summary.footerHex << "\n";
        for (const auto& record : summary.records) {
            output << "record[" << record.recordIndex << "].stableId=" << record.stableId << "\n";
            output << "record[" << record.recordIndex << "].byteOffset=" << record.byteOffset << "\n";
            output << "record[" << record.recordIndex << "].byteLength=" << record.byteLength << "\n";
            output << "record[" << record.recordIndex << "].archiveName=" << record.archiveName << "\n";
            output << "record[" << record.recordIndex << "].machineName=" << record.machineName << "\n";
            output << "record[" << record.recordIndex << "].shareCode=" << record.shareCode << "\n";
        }
    }

    if (!result.diagnostics.empty()) {
        output << "diagnosticCode=" << result.diagnostics.front().code << "\n";
        output << "diagnosticMessage=" << result.diagnostics.front().message << "\n";
        for (const auto& detail : result.diagnostics.front().details) {
            output << "detail=" << detail << "\n";
        }
    }
    return result.diagnostics.empty() ? 0 : 1;
}

int runOpenSaveAcRowsProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto savePath = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    const auto reportPath = std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString());
    auto services = ac6dm::app::AppServices::createDefault();
    const auto result = services.openSaveService->openSourceSave(savePath);

    std::ofstream output(reportPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return 2;
    }

    output << "summary=" << result.session.summary << "\n";
    output << "workflowAvailable=" << (result.session.workflowAvailable ? "true" : "false") << "\n";
    output << "hasRealSave=" << (result.session.hasRealSave ? "true" : "false") << "\n";
    output << "catalogCount=" << result.catalog.size() << "\n";

    int row = 0;
    for (const auto& item : result.catalog) {
        if (item.assetKind != ac6dm::contracts::AssetKind::Ac) {
            continue;
        }
        output << "acItem[" << row << "].assetId=" << item.assetId << "\n";
        output << "acItem[" << row << "].displayName=" << item.displayName << "\n";
        output << "acItem[" << row << "].archiveName=" << item.archiveName << "\n";
        output << "acItem[" << row << "].machineName=" << item.machineName << "\n";
        output << "acItem[" << row << "].shareCode=" << item.shareCode << "\n";
        output << "acItem[" << row << "].originLabel=" << item.sourceLabel << "\n";
        output << "acItem[" << row << "].sourceBucket=" << ac6dm::contracts::toString(item.sourceBucket) << "\n";
        output << "acItem[" << row << "].slotLabel=" << item.slotLabel << "\n";
        output << "acItem[" << row << "].recordRef=" << item.recordRef << "\n";
        output << "acItem[" << row << "].writeCapability=" << ac6dm::contracts::toString(item.writeCapability) << "\n";
        if (item.acPreview.has_value()) {
            output << "acItem[" << row << "].buildLinkCompatible="
                   << (item.acPreview->buildLinkCompatible ? "true" : "false") << "\n";
            for (std::size_t slotIndex = 0; slotIndex < item.acPreview->assemblySlots.size(); ++slotIndex) {
                const auto& slot = item.acPreview->assemblySlots[slotIndex];
                output << "acItem[" << row << "].slot[" << slotIndex << "].key=" << slot.slotKey << "\n";
                output << "acItem[" << row << "].slot[" << slotIndex << "].label=" << slot.slotLabel << "\n";
                output << "acItem[" << row << "].slot[" << slotIndex << "].partName=" << slot.partName << "\n";
                output << "acItem[" << row << "].slot[" << slotIndex << "].manufacturer=" << slot.manufacturer << "\n";
                output << "acItem[" << row << "].slot[" << slotIndex << "].hasMatch=" << (slot.hasMatch ? "true" : "false") << "\n";
                output << "acItem[" << row << "].slot[" << slotIndex << "].agPartId="
                       << (slot.advancedGaragePartId.has_value() ? std::to_string(*slot.advancedGaragePartId) : "-") << "\n";
            }
        }
        for (const auto& field : item.detailFields) {
            output << "acItem[" << row << "].detail." << field.label << "=" << field.value << "\n";
        }
        ++row;
    }

    if (!result.diagnostics.empty()) {
        output << "diagnosticCode=" << result.diagnostics.front().code << "\n";
        output << "diagnosticMessage=" << result.diagnostics.front().message << "\n";
        for (const auto& detail : result.diagnostics.front().details) {
            output << "detail=" << detail << "\n";
        }
    }
    return result.diagnostics.empty() ? 0 : 1;
}

int runImportProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto sourceSave = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    const auto outputSave = std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString());

    if (std::filesystem::exists(outputSave)) {
        std::filesystem::remove(outputSave);
    }
    std::filesystem::copy_file(sourceSave, outputSave, std::filesystem::copy_options::overwrite_existing);

    auto services = ac6dm::app::AppServices::createDefault();
    const auto openResult = services.openSaveService->openSourceSave(outputSave);
    if (!openResult.diagnostics.empty()) {
        std::cout << "openStatus=failed\n";
        for (const auto& detail : openResult.diagnostics.front().details) {
            std::cout << "detail=" << detail << "\n";
        }
        return 1;
    }

    const auto shareIt = std::find_if(openResult.catalog.begin(), openResult.catalog.end(), [](const auto& item) {
        return item.origin == ac6dm::contracts::AssetOrigin::Share;
    });
    if (shareIt == openResult.catalog.end()) {
        std::cout << "importStatus=failed\n";
        std::cout << "detail=no-share-asset-found\n";
        return 1;
    }

    auto plan = services.importPlanningService->buildImportPlan({shareIt->assetId});
    const int targetSlot = plan.suggestedTargetUserSlotIndex.value_or(0);
    plan = services.importPlanningService->buildImportPlan({shareIt->assetId}, targetSlot);
    std::cout << "selectedShareAsset=" << shareIt->assetId << "\n";
    std::cout << "planTargetSlot=" << plan.targetSlot << "\n";
    std::cout << "selectedTargetSlot=" << targetSlot << "\n";
    std::cout << "planBlockerCount=" << plan.blockers.size() << "\n";
    if (!plan.blockers.empty()) {
        for (const auto& blocker : plan.blockers) {
            std::cout << "blocker=" << blocker << "\n";
        }
        return 1;
    }

    ac6dm::contracts::ImportRequestDto request;
    request.sourceAssetIds = {shareIt->assetId};
    request.targetUserSlotIndex = targetSlot;
    const auto importResult = services.importExecutionService->executeImport(request);

    std::cout << "importSummary=" << importResult.action.summary << "\n";
    std::cout << "importStatus=" << importResult.action.status << "\n";
    std::cout << "importCatalogCount=" << importResult.catalog.size() << "\n";
    for (const auto& detail : importResult.action.details) {
        std::cout << "importDetail=" << detail << "\n";
    }
    if (!importResult.diagnostics.empty()) {
        std::cout << "importDiagnostic=" << importResult.diagnostics.front().message << "\n";
        return 1;
    }

    auto reopenServices = ac6dm::app::AppServices::createDefault();
    const auto reopenResult = reopenServices.openSaveService->openSourceSave(outputSave);
    std::cout << "reopenSummary=" << reopenResult.session.summary << "\n";
    std::cout << "reopenCatalogCount=" << reopenResult.catalog.size() << "\n";
    if (!reopenResult.catalog.empty()) {
        std::cout << "reopenFirstAsset=" << reopenResult.catalog.front().assetId << "\n";
    }
    if (!reopenResult.diagnostics.empty()) {
        std::cout << "reopenDiagnostic=" << reopenResult.diagnostics.front().message << "\n";
        for (const auto& detail : reopenResult.diagnostics.front().details) {
            std::cout << "reopenDetail=" << detail << "\n";
        }
        return 1;
    }
    return 0;
}

int runImportAcProbe(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto sourceSave = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString());
    const auto outputSave = std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString());
    const std::optional<int> requestedTargetSlot = argc >= 5
        ? std::optional<int>{QString::fromLocal8Bit(argv[4]).toInt()}
        : std::nullopt;

    if (std::filesystem::exists(outputSave)) {
        std::filesystem::remove(outputSave);
    }
    std::filesystem::copy_file(sourceSave, outputSave, std::filesystem::copy_options::overwrite_existing);

    auto services = ac6dm::app::AppServices::createDefault();
    const auto openResult = services.openSaveService->openSourceSave(outputSave);
    if (!openResult.diagnostics.empty()) {
        std::cout << "openStatus=failed\n";
        for (const auto& detail : openResult.diagnostics.front().details) {
            std::cout << "detail=" << detail << "\n";
        }
        return 1;
    }

    const auto shareIt = std::find_if(openResult.catalog.begin(), openResult.catalog.end(), [](const auto& item) {
        return item.assetKind == ac6dm::contracts::AssetKind::Ac
            && item.origin == ac6dm::contracts::AssetOrigin::Share;
    });
    if (shareIt == openResult.catalog.end()) {
        std::cout << "importStatus=failed\n";
        std::cout << "detail=no-share-ac-found\n";
        return 1;
    }

    auto plan = services.importPlanningService->buildImportPlan({shareIt->assetId});
    const int targetSlotCode = requestedTargetSlot.value_or(plan.suggestedTargetUserSlotIndex.value_or(0));
    plan = services.importPlanningService->buildImportPlan({shareIt->assetId}, targetSlotCode);
    std::cout << "selectedShareAc=" << shareIt->assetId << "\n";
    std::cout << "planTargetSlot=" << plan.targetSlot << "\n";
    std::cout << "selectedTargetSlot=" << targetSlotCode << "\n";
    std::cout << "planBlockerCount=" << plan.blockers.size() << "\n";
    if (!plan.blockers.empty()) {
        for (const auto& blocker : plan.blockers) {
            std::cout << "blocker=" << blocker << "\n";
        }
        return 1;
    }

    const auto targetBucket = targetSlotCode == 0
        ? std::string("user1")
        : targetSlotCode == 1
            ? std::string("user2")
            : targetSlotCode == 2
                ? std::string("user3")
                : std::string("user4");
    const int expectedRecordIndex = static_cast<int>(std::count_if(
        openResult.catalog.begin(),
        openResult.catalog.end(),
        [&](const auto& item) {
            return item.assetKind == ac6dm::contracts::AssetKind::Ac
                && ac6dm::contracts::toString(item.sourceBucket) == targetBucket;
        }));

    ac6dm::contracts::ImportRequestDto request;
    request.sourceAssetIds = {shareIt->assetId};
    request.targetUserSlotIndex = targetSlotCode;
    const auto importResult = services.importExecutionService->executeImport(request);

    std::cout << "importSummary=" << importResult.action.summary << "\n";
    std::cout << "importStatus=" << importResult.action.status << "\n";
    std::cout << "importCatalogCount=" << importResult.catalog.size() << "\n";
    for (const auto& detail : importResult.action.details) {
        std::cout << "importDetail=" << detail << "\n";
    }
    if (!importResult.diagnostics.empty()) {
        std::cout << "importDiagnostic=" << importResult.diagnostics.front().message << "\n";
        return 1;
    }

    auto reopenServices = ac6dm::app::AppServices::createDefault();
    const auto reopenResult = reopenServices.openSaveService->openSourceSave(outputSave);
    std::cout << "reopenSummary=" << reopenResult.session.summary << "\n";
    std::cout << "reopenCatalogCount=" << reopenResult.catalog.size() << "\n";
    if (!reopenResult.diagnostics.empty()) {
        std::cout << "reopenDiagnostic=" << reopenResult.diagnostics.front().message << "\n";
        for (const auto& detail : reopenResult.diagnostics.front().details) {
            std::cout << "reopenDetail=" << detail << "\n";
        }
        return 1;
    }

    const auto reopenedTarget = std::find_if(reopenResult.catalog.begin(), reopenResult.catalog.end(), [&](const auto& item) {
        return item.assetKind == ac6dm::contracts::AssetKind::Ac
            && ac6dm::contracts::toString(item.sourceBucket) == targetBucket
            && item.recordRef.find("record/" + std::to_string(expectedRecordIndex)) != std::string::npos;
    });
    if (reopenedTarget != reopenResult.catalog.end()) {
        std::cout << "reopenTargetAsset=" << reopenedTarget->assetId << "\n";
        std::cout << "reopenTargetRecordRef=" << reopenedTarget->recordRef << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    attachConsoleIfNeeded(argc, argv);
    isolateQtRuntime(argv);
    if (argc >= 3 && std::string_view(argv[1]) == "--probe-open-save") {
        return runOpenSaveProbe(argc, argv);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "--probe-open-save-file") {
        return runOpenSaveFileProbe(argc, argv);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "--probe-open-save-ac-debug") {
        return runOpenSaveAcDebugProbe(argc, argv);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "--probe-open-save-ac-rows") {
        return runOpenSaveAcRowsProbe(argc, argv);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "--probe-import-first-share") {
        return runImportProbe(argc, argv);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "--probe-import-first-share-ac") {
        return runImportAcProbe(argc, argv);
    }
    ac6dm::app::ProductGuiApplication application(argc, argv);
    return application.run();
}
