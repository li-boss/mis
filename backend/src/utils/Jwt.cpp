// =============================================================================
// Jwt — HMAC-SHA256 JWT 签发与验证
//
// 实现参考 RFC 7519，使用内置纯 C++ HMAC-SHA256。
// 无第三方 JWT 库依赖。
// =============================================================================

#include "utils/Jwt.h"
#include "utils/Sha256.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace mis::utils {

namespace {

// ---- Base64URL (RFC 4648 §5) ----
static std::string base64UrlEncode(const unsigned char* data, size_t len)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        unsigned int val = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) val |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) val |= static_cast<unsigned int>(data[i + 2]);

        out.push_back(b64[(val >> 18) & 0x3F]);
        out.push_back(b64[(val >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? b64[(val >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? b64[val & 0x3F] : '=');
    }
    return out;
}

static std::string base64UrlDecode(const std::string& in)
{
    static const signed char b64rev[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

    std::string out;
    out.reserve(in.size());

    unsigned int buf = 0;
    int bits = 0;
    for (char ch : in) {
        if (ch == '=') break;
        signed char val = b64rev[static_cast<unsigned char>(ch)];
        if (val < 0) continue;
        buf = (buf << 6) | static_cast<unsigned int>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

} // anonymous namespace

// ---- 签发 ----
std::string Jwt::sign(int userId, const std::string& username, const std::string& role)
{
    // Header: {"alg":"HS256","typ":"JWT"}
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string headerB64 = base64UrlEncode(
        reinterpret_cast<const unsigned char*>(header.data()), header.size());

    // Payload
    nlohmann::json payload;
    payload["userId"] = userId;
    payload["username"] = username;
    payload["role"] = role;
    payload["exp"] = 2147483647; // "不过期"（2038-01-19）

    std::string payloadStr = payload.dump();
    std::string payloadB64 = base64UrlEncode(
        reinterpret_cast<const unsigned char*>(payloadStr.data()), payloadStr.size());

    std::string signingInput = headerB64 + "." + payloadB64;
    auto sig = HmacSha256::sign(
        reinterpret_cast<const uint8_t*>(SECRET), strlen(SECRET),
        reinterpret_cast<const uint8_t*>(signingInput.data()), signingInput.size());
    std::string sigB64 = base64UrlEncode(sig.data(), sig.size());

    return signingInput + "." + sigB64;
}

// ---- 验签 ----
nlohmann::json Jwt::verify(const std::string& token)
{
    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) throw std::runtime_error("invalid token format (no dot1)");
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) throw std::runtime_error("invalid token format (no dot2)");

    std::string headerB64 = token.substr(0, dot1);
    std::string payloadB64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sigB64 = token.substr(dot2 + 1);

    // 验签
    std::string signingInput = headerB64 + "." + payloadB64;
    auto expectedSig = HmacSha256::sign(
        reinterpret_cast<const uint8_t*>(SECRET), strlen(SECRET),
        reinterpret_cast<const uint8_t*>(signingInput.data()), signingInput.size());
    std::string expectedSigB64 = base64UrlEncode(expectedSig.data(), expectedSig.size());

    // 常量时间比较
    if (sigB64.size() != expectedSigB64.size()) throw std::runtime_error("signature mismatch");
    bool ok = true;
    for (size_t i = 0; i < sigB64.size(); ++i) {
        if (sigB64[i] != expectedSigB64[i]) ok = false;
    }
    if (!ok) throw std::runtime_error("signature mismatch");

    // 解析 payload
    std::string payloadStr = base64UrlDecode(payloadB64);
    return nlohmann::json::parse(payloadStr);
}

} // namespace mis::utils
