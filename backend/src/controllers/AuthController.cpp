// =============================================================================
// AuthController — 用户注册 / 登录 / 信息
// Oracle 优先 + 内存降级（通过 AuthService）
// =============================================================================

#include "controllers/AuthController.hpp"
#include "services/AuthService.hpp"
#include "utils/Encoding.h"
#include "utils/Jwt.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

json userToJson(const mis::services::AuthService::AuthUser& u)
{
    return {
        {"userId", u.userId},
        {"username", u.username},
        {"realName", u.realName},
        {"role", u.role},
        {"roleName", u.roleName}
    };
}

} // namespace

void AuthController::registerRoutes(httplib::Server& server)
{
    // ---- 角色定义 ----
    static const std::vector<std::string> VALID_ROLES = {
        "admin", "keeper", "purchaser"
    };

    // ---- 注册 ----
    server.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            std::string realName = body.value("realName", body.value("real_name", ""));

            if (username.empty() || password.empty()) {
                res.status = 400;
                res.set_content(json{{"code", -1}, {"message", "用户名和密码不能为空"}}.dump(),
                                "application/json");
                return;
            }

            auto u = mis::services::makeAuthService().registerUser(username, password, realName);

            std::string token = mis::utils::Jwt::create({
                {"userId", u.userId},
                {"username", u.username},
                {"realName", u.realName},
                {"role", u.role},
                {"roleName", u.roleName}
            });

            res.status = 201;
            res.set_content(json{
                {"code", 0},
                {"message", "注册成功"},
                {"data", {
                    {"token", token},
                    {"user", userToJson(u)}
                }}
            }.dump(), "application/json");
        } catch (const json::parse_error& ex) {
            res.status = 400;
            res.set_content(json{{"code", -98},
                {"message", std::string("请求数据格式错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        } catch (const std::runtime_error& ex) {
            res.status = 409;
            res.set_content(json{{"code", -2}, {"message", mis::utils::safeError(ex)}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", std::string("服务器内部错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        }
    });

    // ---- 登录 ----
    server.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            auto u = mis::services::makeAuthService().login(username, password);

            std::string token = mis::utils::Jwt::create({
                {"userId", u.userId},
                {"username", u.username},
                {"realName", u.realName},
                {"role", u.role},
                {"roleName", u.roleName}
            });

            res.status = 200;
            res.set_content(json{
                {"code", 0},
                {"message", "登录成功"},
                {"data", {
                    {"token", token},
                    {"user", userToJson(u)}
                }}
            }.dump(), "application/json");
        } catch (const json::parse_error& ex) {
            res.status = 400;
            res.set_content(json{{"code", -98},
                {"message", std::string("请求数据格式错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        } catch (const std::runtime_error& ex) {
            res.status = 401;
            res.set_content(json{{"code", -1}, {"message", mis::utils::safeError(ex)}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", std::string("服务器内部错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        }
    });

    // ---- 获取当前用户 ----
    server.Get("/api/auth/me", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth = req.has_header("Authorization")
                           ? req.get_header_value("Authorization") : "";
        if (auth.rfind("Bearer ", 0) == 0) {
            auth = auth.substr(7);
        }

        if (auth.empty()) {
            res.status = 401;
            res.set_content(json{{"code", -1}, {"message", "未登录"}}.dump(),
                            "application/json");
            return;
        }

        try {
            auto claims = mis::utils::Jwt::verify(auth);
            res.status = 200;
            res.set_content(json{
                {"code", 0},
                {"data", {
                    {"user", {
                        {"userId", claims["userId"]},
                        {"username", claims["username"]},
                        {"realName", claims["realName"]},
                        {"role", claims["role"]},
                        {"roleName", claims["roleName"]}
                    }}
                }}
            }.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 401;
            res.set_content(json{{"code", -1},
                {"message", std::string("token 无效或已过期: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        }
    });

    // ---- 退出登录（无状态，直接返回成功） ----
    server.Post("/api/auth/logout", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(json{{"code", 0}, {"message", "已退出"}}.dump(),
                        "application/json");
    });

    // ---- 从请求中提取 JWT claims 并校验 admin 角色 ----
    auto requireAdmin = [](const httplib::Request& req, httplib::Response& res) -> json {
        std::string auth = req.has_header("Authorization")
                           ? req.get_header_value("Authorization") : "";
        if (auth.rfind("Bearer ", 0) != 0) {
            res.status = 401;
            res.set_content(json{{"code", -1}, {"message", "未登录"}}.dump(),
                            "application/json");
            return nullptr;
        }
        try {
            auto claims = mis::utils::Jwt::verify(auth.substr(7));
            if (claims.value("role", "") != "admin") {
                res.status = 403;
                res.set_content(json{{"code", -3},
                    {"message", "仅管理员可执行此操作"}}.dump(), "application/json");
                return nullptr;
            }
            return claims;
        } catch (const std::exception& ex) {
            res.status = 401;
            res.set_content(json{{"code", -1},
                {"message", std::string("token 无效: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
            return nullptr;
        }
    };

    // ---- 用户列表（admin） ----
    server.Get("/api/users", [=](const httplib::Request& req, httplib::Response& res) {
        auto claims = requireAdmin(req, res);
        if (claims.is_null()) return;

        try {
            auto users = mis::services::makeAuthService().listAll();
            json list = json::array();
            for (const auto& u : users) {
                list.push_back(userToJson(u));
            }
            res.status = 200;
            res.set_content(json{{"code", 0}, {"data", {{"list", list}}}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", std::string("服务器内部错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        }
    });

    // ---- 修改用户角色（admin） ----
    server.Put(R"(/api/users/(\d+)/role)", [=](const httplib::Request& req, httplib::Response& res) {
        auto claims = requireAdmin(req, res);
        if (claims.is_null()) return;

        int targetId = std::stoi(req.matches[1]);

        try {
            auto body = json::parse(req.body);
            std::string newRole = body.value("role", "");

            if (std::find(VALID_ROLES.begin(), VALID_ROLES.end(), newRole)
                == VALID_ROLES.end()) {
                res.status = 400;
                res.set_content(json{{"code", -4},
                    {"message", "无效的角色: " + newRole}}.dump(), "application/json");
                return;
            }

            int adminId = claims["userId"].get<int>();
            if (targetId == adminId && newRole != "admin") {
                res.status = 403;
                res.set_content(json{{"code", -6},
                    {"message", "不可降级自己的管理员角色"}}.dump(), "application/json");
                return;
            }

            mis::services::makeAuthService().updateRole(targetId, newRole);

            auto updated = mis::services::makeAuthService().getById(targetId);

            res.status = 200;
            res.set_content(json{
                {"code", 0},
                {"message", "角色已更新"},
                {"data", {{"user", userToJson(updated)}}}
            }.dump(), "application/json");
        } catch (const json::parse_error& ex) {
            res.status = 400;
            res.set_content(json{{"code", -98},
                {"message", std::string("请求数据格式错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        } catch (const std::runtime_error& ex) {
            res.status = 404;
            res.set_content(json{{"code", -5}, {"message", mis::utils::safeError(ex)}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", std::string("服务器内部错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        }
    });
}

void registerAuthRoutes(httplib::Server& server)
{
    AuthController{}.registerRoutes(server);
}

} // namespace mis::controllers
