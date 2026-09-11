#include "crypto.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

#include <array>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "advapi32.lib")

static std::string BytesToHex(const unsigned char* data, size_t len, bool upper) {
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

static bool Sha256Raw(const unsigned char* data, size_t len, unsigned char out[32]) {
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return false;
    }

    DWORD hashLen = 32;
    const bool ok = CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)
        && CryptHashData(hash, data, static_cast<DWORD>(len), 0)
        && CryptGetHashParam(hash, HP_HASHVAL, out, &hashLen, 0);

    if (hash) {
        CryptDestroyHash(hash);
    }
    CryptReleaseContext(prov, 0);
    return ok;
}

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
