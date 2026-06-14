// =============================================================================
// AuthController — 用户注册 / 登录 / 信息
// 内存存储（演示用途）
// =============================================================================

#include "controllers/AuthController.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

struct User {
    int id;
    std::string username;
    std::string password;     // 明文（演示用，生产应 hash）
    std::string realName;
    std::string role;
    std::string roleName;
};

std::vector<User> users;
std::mutex userMutex;
int nextUserId = 1;

void initDemoUsers()
{
    if (!users.empty()) return;
    users.push_back({nextUserId++, "admin",    "123456", "管理员", "admin",    "管理员"});
    users.push_back({nextUserId++, "operator", "123456", "操作员", "operator", "仓储操作员"});
    users.push_back({nextUserId++, "baiqinhe", "123456", "白沁禾", "frontend", "前端核心"});
}

std::string makeToken(int userId, const std::string& username)
{
    return "wms-token-" + std::to_string(userId) + "-" + username;
}

json userToJson(const User& u)
{
    return {
        {"userId", u.id},
        {"username", u.username},
        {"realName", u.realName},
        {"role", u.role},
        {"roleName", u.roleName}
    };
}

} // namespace

void AuthController::registerRoutes(httplib::Server& server)
{
    initDemoUsers();

    // ---- 注册 ----
    server.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            std::string realName = body.value("realName", body.value("real_name", ""));

            if (username.empty() || password.empty()) {
                res.status = 400;
                res.set_content(json{{"code", -1}, {"message", "用户名和密码不能为空"}}.dump(), "application/json");
                return;
            }

            std::lock_guard<std::mutex> lock(userMutex);

            auto it = std::find_if(users.begin(), users.end(),
                [&](const User& u) { return u.username == username; });
            if (it != users.end()) {
                res.status = 409;
                res.set_content(json{{"code", -2}, {"message", "用户名已存在"}}.dump(), "application/json");
                return;
            }

            User u;
            u.id = nextUserId++;
            u.username = username;
            u.password = password;
            u.realName = realName.empty() ? username : realName;
            u.role = "operator";
            u.roleName = "仓储操作员";
            users.push_back(u);

            std::string token = makeToken(u.id, u.username);

            res.status = 201;
            res.set_content(json{
                {"code", 0},
                {"message", "注册成功"},
                {"data", {
                    {"token", token},
                    {"user", userToJson(u)}
                }}
            }.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99}, {"message", ex.what()}}.dump(), "application/json");
        }
    });

    // ---- 登录 ----
    server.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            std::lock_guard<std::mutex> lock(userMutex);

            auto it = std::find_if(users.begin(), users.end(),
                [&](const User& u) { return u.username == username && u.password == password; });

            if (it == users.end()) {
                res.status = 401;
                res.set_content(json{{"code", -1}, {"message", "用户名或密码错误"}}.dump(), "application/json");
                return;
            }

            std::string token = makeToken(it->id, it->username);

            res.status = 200;
            res.set_content(json{
                {"code", 0},
                {"message", "登录成功"},
                {"data", {
                    {"token", token},
                    {"user", userToJson(*it)}
                }}
            }.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99}, {"message", ex.what()}}.dump(), "application/json");
        }
    });

    // ---- 获取当前用户 ----
    server.Get("/api/auth/me", [](const httplib::Request& req, httplib::Response& res) {
        // 从 Authorization header 解析 token
        std::string auth = req.has_header("Authorization") ? req.get_header_value("Authorization") : "";
        if (auth.rfind("Bearer ", 0) == 0) {
            auth = auth.substr(7);
        }

        if (auth.empty()) {
            res.status = 401;
            res.set_content(json{{"code", -1}, {"message", "未登录"}}.dump(), "application/json");
            return;
        }

        std::lock_guard<std::mutex> lock(userMutex);
        for (const auto& u : users) {
            if (makeToken(u.id, u.username) == auth) {
                res.status = 200;
                res.set_content(json{{"code", 0}, {"data", {{"user", userToJson(u)}}}}.dump(), "application/json");
                return;
            }
        }

        res.status = 401;
        res.set_content(json{{"code", -1}, {"message", "token 无效"}}.dump(), "application/json");
    });

    // ---- 退出登录（无状态，直接返回成功） ----
    server.Post("/api/auth/logout", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(json{{"code", 0}, {"message", "已退出"}}.dump(), "application/json");
    });
}

void registerAuthRoutes(httplib::Server& server)
{
    AuthController{}.registerRoutes(server);
}

} // namespace mis::controllers
