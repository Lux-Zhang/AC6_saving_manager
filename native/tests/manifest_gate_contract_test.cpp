#include "config/sidecar/ThirdPartyManifest.hpp"

#include <gtest/gtest.h>

TEST(ManifestGateContractTest, ResultDefaultsToBlocked) {
    ac6dm::sidecar::ManifestVerificationResult result;
    EXPECT_FALSE(result.success);
}
