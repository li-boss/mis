#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace mis::utils {

class Jwt {
public:
    /// 签发 JWT（HS256），payload 中嵌入 userId / username / role
    static std::string sign(int userId, const std::string& username, const std::string& role);

    /// 验签并解析 payload，失败抛异常
    static nlohmann::json verify(const std::string& token);

private:
    // HMAC-SHA256 签名密钥（正式环境应从配置文件/环境变量读取）
    static constexpr const char* SECRET = "mis-wms-secret-key-2026";
};

} // namespace mis::utils
