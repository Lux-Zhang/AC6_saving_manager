#pragma once

#include <string_view>

namespace ac6dm::contracts::errors {

constexpr std::string_view kOpenSaveBlockedCode = "save.open.blocked";
constexpr std::string_view kManifestBlockedCode = "sidecar.manifest.blocked";
constexpr std::string_view kSidecarInvocationFailedCode = "sidecar.invoke.failed";
constexpr std::string_view kNoEmptyUserSlotCode = "import.no-empty-user-slot";
constexpr std::string_view kReadbackFailedCode = "import.readback.failed";
constexpr std::string_view kOutputCollisionCode = "publish.output-collision";

constexpr std::string_view kOpenSaveFailedUserMessage = "无法打开该存档，请确认文件未被占用。";
constexpr std::string_view kSaveFailedUserMessage = "保存失败，未生成新存档。";
constexpr std::string_view kCopyErrorDetailsLabel = "复制错误详情";
constexpr std::string_view kOpenSupportBundleDirectoryLabel = "打开支持包目录";

}  // namespace ac6dm::contracts::errors
