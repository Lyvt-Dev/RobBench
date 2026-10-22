#include "updates/UpdateManifest.h"
#include <QCryptographicHash>
#include <gtest/gtest.h>
using namespace rb;
TEST(Updates, VersionOrderingAndRejection) {
    EXPECT_LT(parseVersion("v0.2.0"), parseVersion("1.0.0"));
    EXPECT_GT(parseVersion("0.10.0"), parseVersion("0.2.9"));
    EXPECT_THROW(parseVersion("1.2.3.exe"), std::runtime_error);
    EXPECT_THROW(parseVersion("v1.2"), std::runtime_error);
}
TEST(Updates, ChecksumRequiresExactFileAndRejectsDuplicates) {
    auto digest = QCryptographicHash::hash("abc", QCryptographicHash::Sha256).toHex().toStdString();
    EXPECT_EQ(checksumFor(digest + "  RobBench.exe\n", "RobBench.exe"), digest);
    EXPECT_THROW(checksumFor(digest + "  Other.exe", "RobBench.exe"), std::runtime_error);
    EXPECT_THROW(checksumFor("1234  RobBench.exe", "RobBench.exe"), std::runtime_error);
    EXPECT_THROW(checksumFor(digest + "  a.exe\n" + digest + "  a.exe", "a.exe"), std::runtime_error);
}
TEST(Updates, ManifestRequiresInstallerAndChecksums) {
    nlohmann::json j = {{"tag_name", "v0.2.0"}, {"assets", nlohmann::json::array()}};
    EXPECT_THROW(parseUpdate(j), std::runtime_error);
    j["assets"].push_back({{"name", "RobBench-0.2.0-win64.exe"},
                           {"size", 123},
                           {"browser_download_url", "https://github.com/a"},
                           {"url", "https://api.github.com/a"}});
    j["assets"].push_back({{"name", "SHA256SUMS"},
                           {"browser_download_url", "https://github.com/b"},
                           {"url", "https://api.github.com/b"}});
    auto info = parseUpdate(j);
    EXPECT_EQ(info.filename, "RobBench-0.2.0-win64.exe");
    EXPECT_EQ(info.size, 123);
}
