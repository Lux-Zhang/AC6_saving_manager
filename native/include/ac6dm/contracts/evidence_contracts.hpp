#pragma once

#include <string_view>

namespace ac6dm::contracts::evidence {

constexpr std::string_view kAcceptanceRoot = "artifacts/acceptance";
constexpr std::string_view kN0Root = "artifacts/acceptance/n0";
constexpr std::string_view kSidecarQualificationReport = "artifacts/acceptance/n0/sidecar-qualification-report.json";
constexpr std::string_view kManifestGateResult = "artifacts/acceptance/n0/manifest-gate-result.json";
constexpr std::string_view kEvidenceManifest = "artifacts/acceptance/evidence-manifest.json";

}  // namespace ac6dm::contracts::evidence
