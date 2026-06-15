// =============================================================================
// Sha256 — 纯 C++ SHA-256 与 HMAC-SHA256 实现
//
// 严格遵循 FIPS 180-4 和 RFC 2104。
// 无第三方依赖，可直接编译。
// =============================================================================

#include "utils/Sha256.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace mis::utils {

namespace {

// ---- SHA-256 常数（前 64 个素数的立方根的小数部分的前 32 位） ----
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint32_t sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static inline uint32_t sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static inline uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static inline uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitCount;
    uint8_t buffer[64];
    size_t bufferLen;
};

static void sha256Transform(Sha256Ctx& ctx, const uint8_t* block) {
    uint32_t W[64];
    for (int t = 0; t < 16; ++t) {
        W[t] = (static_cast<uint32_t>(block[t * 4]) << 24) |
               (static_cast<uint32_t>(block[t * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[t * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[t * 4 + 3]);
    }
    for (int t = 16; t < 64; ++t) {
        W[t] = gamma1(W[t - 2]) + W[t - 7] + gamma0(W[t - 15]) + W[t - 16];
    }

    uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2];
    uint32_t d = ctx.state[3], e = ctx.state[4], f = ctx.state[5];
    uint32_t g = ctx.state[6], h = ctx.state[7];

    for (int t = 0; t < 64; ++t) {
        uint32_t T1 = h + sigma1(e) + ch(e, f, g) + K[t] + W[t];
        uint32_t T2 = sigma0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
    }

    ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
}

static std::vector<uint8_t> sha256Final(Sha256Ctx& ctx) {
    // Padding
    uint64_t bits = ctx.bitCount;
    ctx.buffer[ctx.bufferLen++] = 0x80;
    if (ctx.bufferLen > 56) {
        memset(ctx.buffer + ctx.bufferLen, 0, 64 - ctx.bufferLen);
        sha256Transform(ctx, ctx.buffer);
        ctx.bufferLen = 0;
    }
    memset(ctx.buffer + ctx.bufferLen, 0, 56 - ctx.bufferLen);

    // Append bit count in big-endian
    for (int i = 0; i < 8; ++i) {
        ctx.buffer[56 + i] = static_cast<uint8_t>((bits >> (56 - i * 8)) & 0xFF);
    }
    sha256Transform(ctx, ctx.buffer);

    std::vector<uint8_t> digest(32);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4]     = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
    }
    return digest;
}

} // anonymous namespace

std::vector<uint8_t> Sha256::hash(const uint8_t* data, size_t len) {
    Sha256Ctx ctx;
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.bitCount = 0;
    ctx.bufferLen = 0;

    size_t totalProcessed = 0;
    while (totalProcessed < len) {
        size_t remaining = len - totalProcessed;
        size_t space = 64 - ctx.bufferLen;
        size_t copyLen = std::min(remaining, space);
        memcpy(ctx.buffer + ctx.bufferLen, data + totalProcessed, copyLen);
        ctx.bufferLen += copyLen;
        totalProcessed += copyLen;
        ctx.bitCount += copyLen * 8;
        if (ctx.bufferLen == 64) {
            sha256Transform(ctx, ctx.buffer);
            ctx.bufferLen = 0;
        }
    }

    return sha256Final(ctx);
}

std::string Sha256::hashHex(const std::string& input) {
    auto digest = hash(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    std::ostringstream oss;
    for (uint8_t b : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

// ---- HMAC-SHA256 (RFC 2104) ----
std::vector<uint8_t> HmacSha256::sign(const uint8_t* key, size_t keyLen,
                                       const uint8_t* data, size_t dataLen) {
    // 如果 key 长度 > 64，先对 key 做 SHA-256
    uint8_t k[64];
    memset(k, 0, 64);
    if (keyLen > 64) {
        auto hashedKey = Sha256::hash(key, keyLen);
        memcpy(k, hashedKey.data(), std::min(hashedKey.size(), size_t(64)));
    } else {
        memcpy(k, key, keyLen);
    }

    // ipad = k ^ 0x36
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    // inner = SHA256(ipad || message)
    std::vector<uint8_t> innerInput;
    innerInput.reserve(64 + dataLen);
    innerInput.insert(innerInput.end(), ipad, ipad + 64);
    innerInput.insert(innerInput.end(), data, data + dataLen);
    auto innerHash = Sha256::hash(innerInput.data(), innerInput.size());

    // outer = SHA256(opad || innerHash)
    std::vector<uint8_t> outerInput;
    outerInput.reserve(64 + 32);
    outerInput.insert(outerInput.end(), opad, opad + 64);
    outerInput.insert(outerInput.end(), innerHash.begin(), innerHash.end());
    return Sha256::hash(outerInput.data(), outerInput.size());
}

} // namespace mis::utils
