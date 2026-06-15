#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mis::utils {

/// 纯 C++ SHA-256 实现
class Sha256 {
public:
    /// 计算 SHA-256 摘要，返回 32 字节原始数据
    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);

    /// 计算 SHA-256 摘要，返回 64 字符 hex 字符串
    static std::string hashHex(const std::string& input);
};

/// 纯 C++ HMAC-SHA256 实现
class HmacSha256 {
public:
    /// 计算 HMAC-SHA256，返回 32 字节原始数据
    static std::vector<uint8_t> sign(const uint8_t* key, size_t keyLen,
                                     const uint8_t* data, size_t dataLen);
};

} // namespace mis::utils
