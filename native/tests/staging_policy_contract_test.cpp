#include "core/workspace/SessionWorkspace.hpp"

#include <gtest/gtest.h>

TEST(StagingPolicyContractTest, AsciiDetectionRejectsUnicodePath) {
    EXPECT_FALSE(ac6dm::workspace::SessionWorkspace::isAsciiOnly(std::filesystem::path("/tmp/测试")));
    EXPECT_TRUE(ac6dm::workspace::SessionWorkspace::isAsciiOnly(std::filesystem::path("/tmp/AC6DM/runtime")));
}
