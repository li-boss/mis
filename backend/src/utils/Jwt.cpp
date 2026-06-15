#include "utils/Jwt.h"
#include <bcrypt.h>

#include <stdexcept>
#include <chrono>
#include <vector>
#include <cstring>

namespace mis::utils {

// ---- Base64URL 编解码 ----

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Jwt::base64UrlEncode(const std::string& data)
{
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        uint32_t triple = (static_cast<unsigned char>(data[i]) << 16)
                        | (static_cast<unsigned char>(data[i + 1]) << 8)
                        |  static_cast<unsigned char>(data[i + 2]);
        result.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        result.push_back(BASE64_CHARS[(triple >> 6)  & 0x3F]);
        result.push_back(BASE64_CHARS[ triple        & 0x3F]);
    }

    // 处理剩余字节
    if (i < data.size()) {
        uint32_t triple = static_cast<unsigned char>(data[i]) << 16;
        if (i + 1 < data.size()) {
            triple |= static_cast<unsigned char>(data[i + 1]) << 8;
        }
        result.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        if (i + 1 < data.size()) {
            result.push_back(BASE64_CHARS[(triple >> 6) & 0x3F]);
        } else {
            result.push_back('=');
        }
        result.push_back('=');
    }

    // 转为 Base64URL: + → -, / → _, 去尾部 =
    for (char& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!result.empty() && result.back() == '=') {
        result.pop_back();
    }

    return result;
}

std::string Jwt::base64UrlDecode(const std::string& data)
{
    // Base64URL → 标准 Base64
    std::string b64 = data;
    for (char& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    // 补齐 padding
    while (b64.size() % 4 != 0) {
        b64.push_back('=');
    }

    // 解码表
    static int decodeTable[256] = {};
    static bool tableBuilt = false;
    if (!tableBuilt) {
        for (int j = 0; j < 256; ++j) decodeTable[j] = -1;
        for (int j = 0; j < 64; ++j) decodeTable[static_cast<unsigned char>(BASE64_CHARS[j])] = j;
        tableBuilt = true;
    }

    std::string result;
    result.reserve((b64.size() / 4) * 3);

    for (size_t i = 0; i < b64.size(); i += 4) {
        int a = decodeTable[static_cast<unsigned char>(b64[i])];
        int b = decodeTable[static_cast<unsigned char>(b64[i + 1])];
        int c = (b64[i + 2] == '=') ? 0 : decodeTable[static_cast<unsigned char>(b64[i + 2])];
        int d = (b64[i + 3] == '=') ? 0 : decodeTable[static_cast<unsigned char>(b64[i + 3])];

        if (a < 0 || b < 0) {
            throw std::runtime_error("Invalid base64 input");
        }

        uint32_t triple = (static_cast<uint32_t>(a) << 18)
                        | (static_cast<uint32_t>(b) << 12)
                        | (static_cast<uint32_t>(c) << 6)
                        |  static_cast<uint32_t>(d);

        result.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (b64[i + 2] != '=') {
            result.push_back(static_cast<char>((triple >> 8) & 0xFF));
        }
        if (b64[i + 3] != '=') {
            result.push_back(static_cast<char>(triple & 0xFF));
        }
    }

    return result;
}

// ---- HMAC-SHA256 via Windows CNG ----

std::string Jwt::hmacSha256(const std::string& key, const std::string& data)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status;

    // 打开 SHA-256 算法提供程序（HMAC 模式）
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                          BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed: " +
                                 std::to_string(status));
    }

    // 创建 HMAC hash 对象
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0,
                              reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
                              static_cast<ULONG>(key.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptCreateHash failed: " + std::to_string(status));
    }

    // 喂入数据
    status = BCryptHashData(hHash,
                            reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                            static_cast<ULONG>(data.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptHashData failed: " + std::to_string(status));
    }

    // 获取哈希长度
    DWORD hashSize = 0;
    ULONG cbResult = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
                               reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize),
                               &cbResult, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGetProperty failed: " + std::to_string(status));
    }

    // 完成哈希
    std::vector<unsigned char> hash(hashSize);
    status = BCryptFinishHash(hHash, hash.data(), hashSize, 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("BCryptFinishHash failed: " + std::to_string(status));
    }

    return std::string(reinterpret_cast<char*>(hash.data()), hash.size());
}

// ---- JWT 创建与验证 ----

std::string Jwt::create(const nlohmann::json& payload, const std::string& secret)
{
    using namespace nlohmann;

    // JWT Header
    json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };

    // 构建 payload（添加时间戳）
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                          now.time_since_epoch()).count();
    auto expSeconds = nowSeconds + 86400; // 24 小时有效

    json fullPayload = payload;
    fullPayload["iat"] = nowSeconds;
    fullPayload["exp"] = expSeconds;

    // 编码三段
    std::string headerB64 = base64UrlEncode(header.dump());
    std::string payloadB64 = base64UrlEncode(fullPayload.dump());
    std::string signingInput = headerB64 + "." + payloadB64;

    std::string signature = base64UrlEncode(hmacSha256(secret, signingInput));

    return signingInput + "." + signature;
}

nlohmann::json Jwt::verify(const std::string& token, const std::string& secret)
{
    using namespace nlohmann;

    // 拆分 token 为三段
    size_t firstDot = token.find('.');
    size_t secondDot = token.rfind('.');

    if (firstDot == std::string::npos || secondDot == std::string::npos ||
        firstDot == secondDot) {
        throw std::runtime_error("Invalid token format");
    }

    std::string headerB64 = token.substr(0, firstDot);
    std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string signatureB64 = token.substr(secondDot + 1);

    // 验证签名
    std::string signingInput = headerB64 + "." + payloadB64;
    std::string expectedSig = base64UrlEncode(hmacSha256(secret, signingInput));

    if (signatureB64 != expectedSig) {
        throw std::runtime_error("Invalid token signature");
    }

    // 解码 payload
    std::string payloadJson = base64UrlDecode(payloadB64);
    json claims = json::parse(payloadJson);

    // 检查过期
    if (claims.contains("exp")) {
        auto now = std::chrono::system_clock::now();
        auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                              now.time_since_epoch()).count();
        if (claims["exp"].get<int64_t>() < nowSeconds) {
            throw std::runtime_error("Token expired");
        }
    }

    return claims;
}

} // namespace mis::utils
