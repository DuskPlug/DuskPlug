#include "../src/crypto.h"
#include "test_assert.h"

#include <string>

TEST(Sha256EmptyBodyMatchesTuya) {
    const std::string hash = Sha256HexLower("");
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(HmacSha256ProducesUpperHex) {
    const std::string sign = HmacSha256HexUpper("secret", "message");
    EXPECT_EQ(sign.size(), 64u);
    EXPECT_TRUE(sign.find('A') != std::string::npos || sign.find('F') != std::string::npos);
}

void RunCryptoTests() {
    std::printf("crypto tests\n");
    RUN_TEST(Sha256EmptyBodyMatchesTuya);
    RUN_TEST(HmacSha256ProducesUpperHex);
}
