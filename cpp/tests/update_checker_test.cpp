#include "../src/json_util.h"
#include "../src/semver.h"
#include "test_assert.h"

#include <string>

TEST(UpdateManifestAssetFields) {
    const std::string manifest = R"({
  "version": "9.9.9",
  "notes_url": "https://example.com/release",
  "assets": {
    "windows_zip": {
      "url": "https://example.com/DuskPlug-Windows.zip",
      "sha256": "abc123"
    }
  }
})";

    EXPECT_EQ(JsonGetAssetField(manifest, "windows_zip", "url").value_or(""), "https://example.com/DuskPlug-Windows.zip");
    EXPECT_EQ(JsonGetAssetField(manifest, "windows_zip", "sha256").value_or(""), "abc123");
}

TEST(UpdateVersionComparison) {
    EXPECT_TRUE(IsNewerVersion("9.9.9", "1.0.1"));
}

void RunUpdateCheckerTests() {
    std::printf("update checker tests\n");
    RUN_TEST(UpdateManifestAssetFields);
    RUN_TEST(UpdateVersionComparison);
}
