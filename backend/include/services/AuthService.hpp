#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace mis::services {

class AuthService {
public:
    struct AuthUser {
        int userId{0};
        std::string username;
        std::string password;       // 演示用明文，映射 DB password_hash
        std::string realName;       // 映射 DB real_name
        std::string role;
        std::string roleName;       // 派生字段，不存 DB
    };

    // 登录：成功返回用户，失败抛 std::runtime_error
    AuthUser login(const std::string& username, const std::string& password);

    // 注册：成功返回用户，用户名重复抛 std::runtime_error
    AuthUser registerUser(const std::string& username, const std::string& password,
                          const std::string& realName);

    // 按 ID 查询
    AuthUser getById(int userId);

    // 全量列表
    std::vector<AuthUser> listAll();

    // 更新角色
    void updateRole(int userId, const std::string& newRole);

    // 角色 → 中文名
    static std::string roleToName(const std::string& role);
};

AuthService makeAuthService();

} // namespace mis::services
