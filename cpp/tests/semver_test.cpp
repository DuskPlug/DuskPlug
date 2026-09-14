#include "../src/semver.h"
#include "test_assert.h"

TEST(ParseSemVerBasic) {
    const auto parsed = ParseSemVer("1.2.3");
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->major, 1);
    EXPECT_EQ(parsed->minor, 2);
    EXPECT_EQ(parsed->patch, 3);
}

TEST(ParseSemVerTagged) {
    const auto tagged = ParseSemVer("v2.0.0");
    EXPECT_TRUE(tagged.has_value());
    EXPECT_EQ(tagged->major, 2);
}

TEST(ParseSemVerRejectsPartial) {
    EXPECT_FALSE(ParseSemVer("1.2").has_value());
}

TEST(CompareSemVerEqual) {
    const auto parsed = ParseSemVer("1.2.3");
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(CompareSemVer(*parsed, *parsed), 0);
}

TEST(IsNewerVersionChecks) {
    EXPECT_TRUE(IsNewerVersion("1.2.4", "1.2.3"));
    EXPECT_FALSE(IsNewerVersion("1.2.3", "1.2.3"));
    EXPECT_FALSE(IsNewerVersion("1.2.2", "1.2.3"));
}

void RunSemverTests() {
    std::printf("semver tests\n");
    RUN_TEST(ParseSemVerBasic);
    RUN_TEST(ParseSemVerTagged);
    RUN_TEST(ParseSemVerRejectsPartial);
    RUN_TEST(CompareSemVerEqual);
    RUN_TEST(IsNewerVersionChecks);
}
