// =============================================================================
// Jwt.h — 轻量 JWT 工具（HS256，基于 Windows CNG）
//
// 使用方式：
//   auto token = mis::utils::Jwt::create({{"userId", 1}, {"username", "admin"}});
//   auto claims = mis::utils::Jwt::verify(token);
// =============================================================================

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace mis::utils {

class Jwt
{
public:
    // JWT 签名密钥（生产应从配置文件或环境变量读取）
    static constexpr const char* SECRET = "wms-jwt-secret-key-2026";

    // 创建 JWT token
    // @param payload  自定义 claims（会自动添加 iat / exp）
    // @param secret   签名密钥
    // @return         三段式 JWT 字符串
    static std::string create(const nlohmann::json& payload,
                              const std::string& secret = SECRET);

    // 验证 JWT token 并返回 claims
    // @param token   三段式 JWT 字符串
    // @param secret  签名密钥
    // @return        payload claims（若验证失败则抛出 std::runtime_error）
    static nlohmann::json verify(const std::string& token,
                                 const std::string& secret = SECRET);

private:
    static std::string base64UrlEncode(const std::string& data);
    static std::string base64UrlDecode(const std::string& data);
    static std::string hmacSha256(const std::string& key, const std::string& data);
};

} // namespace mis::utils
