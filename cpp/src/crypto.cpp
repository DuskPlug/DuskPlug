#include "crypto.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kSha256Init[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

uint32_t RotR(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t Sigma0(uint32_t x) {
    return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22);
}

uint32_t Sigma1(uint32_t x) {
    return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25);
}

uint32_t sigma0(uint32_t x) {
    return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3);
}

uint32_t sigma1(uint32_t x) {
    return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10);
}

void Sha256Transform(uint32_t state[8], const unsigned char block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
            | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
            | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
            | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t t1 = h + Sigma1(e) + Ch(e, f, g) + kSha256K[i] + w[i];
        const uint32_t t2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

bool Sha256Raw(const unsigned char* data, size_t len, unsigned char out[32]) {
    uint32_t state[8];
    memcpy(state, kSha256Init, sizeof(state));

    size_t total = 0;
    unsigned char block[64]{};

    while (total + 64 <= len) {
        Sha256Transform(state, data + total);
        total += 64;
    }

    const size_t rem = len - total;
    memcpy(block, data + total, rem);
    block[rem] = 0x80;

    if (rem >= 56) {
        Sha256Transform(state, block);
        memset(block, 0, sizeof(block));
    }

    const uint64_t bitLen = static_cast<uint64_t>(len) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        block[63 - i] = static_cast<unsigned char>(bitLen >> (8 * i));
    }
    Sha256Transform(state, block);

    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<unsigned char>(state[i] >> 24);
        out[i * 4 + 1] = static_cast<unsigned char>(state[i] >> 16);
        out[i * 4 + 2] = static_cast<unsigned char>(state[i] >> 8);
        out[i * 4 + 3] = static_cast<unsigned char>(state[i]);
    }
    return true;
}

std::string BytesToHex(const unsigned char* data, size_t len, bool upper) {
    static const char* lower = "0123456789abcdef";
    static const char* upperHex = "0123456789ABCDEF";
    const char* table = upper ? upperHex : lower;
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(table[(data[i] >> 4) & 0xF]);
        out.push_back(table[data[i] & 0xF]);
    }
    return out;
}

}  // namespace

std::string Sha256HexLower(const std::string& text) {
    unsigned char hash[32]{};
    if (!Sha256Raw(reinterpret_cast<const unsigned char*>(text.data()), text.size(), hash)) {
        return {};
    }
    return BytesToHex(hash, 32, false);
}

std::string HmacSha256HexUpper(const std::string& key, const std::string& message) {
    constexpr size_t blockSize = 64;
    std::array<unsigned char, blockSize> keyBlock{};
    std::array<unsigned char, blockSize> ipad{};
    std::array<unsigned char, blockSize> opad{};

    if (key.size() > blockSize) {
        unsigned char hashedKey[32]{};
        if (!Sha256Raw(reinterpret_cast<const unsigned char*>(key.data()), key.size(), hashedKey)) {
            return {};
        }
        memcpy(keyBlock.data(), hashedKey, 32);
    } else {
        memcpy(keyBlock.data(), key.data(), key.size());
    }

    for (size_t i = 0; i < blockSize; ++i) {
        ipad[i] = keyBlock[i] ^ 0x36;
        opad[i] = keyBlock[i] ^ 0x5c;
    }

    std::vector<unsigned char> inner;
    inner.reserve(blockSize + message.size());
    inner.insert(inner.end(), ipad.begin(), ipad.end());
    inner.insert(inner.end(), message.begin(), message.end());

    unsigned char innerHash[32]{};
    if (!Sha256Raw(inner.data(), inner.size(), innerHash)) {
        return {};
    }

    std::vector<unsigned char> outer;
    outer.reserve(blockSize + 32);
    outer.insert(outer.end(), opad.begin(), opad.end());
    outer.insert(outer.end(), innerHash, innerHash + 32);

    unsigned char finalHash[32]{};
    if (!Sha256Raw(outer.data(), outer.size(), finalHash)) {
        return {};
    }

    return BytesToHex(finalHash, 32, true);
}
