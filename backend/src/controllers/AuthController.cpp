// =============================================================================
// AuthController — 用户认证 REST 路由
//
// 提供 /api/auth/login, /api/auth/register, /api/auth/logout, /api/auth/me 四个端点。
// 密码使用 SHA-256 摘要存储（演示级，生产环境应使用 bcrypt）。
// Oracle 模式：读写 users 表；内存降级：内存存储用户数据。
// =============================================================================

#include "controllers/AuthController.hpp"
#include "utils/Jwt.h"
#include "utils/Sha256.hpp"
#include "utils/RequestContext.hpp"

#include <nlohmann/json.hpp>

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mis::controllers {

using json = nlohmann::json;

// =========================================================================
// 用户存储
// =========================================================================

struct UserRecord {
    int userId{0};
    std::string username;
    std::string passwordHash; // SHA-256 hex
    std::string role{"operator"};
};

#ifdef MIS_HAS_ORACLE
static int oracleRegister(const std::string& username, const std::string& passwordHash,
                          const std::string& role)
{
    mis::dao::DbSessionGuard db;
    auto seqRow = mis::dao::oracle().query("SELECT seq_users.NEXTVAL AS ID FROM DUAL");
    int id = std::stoi(seqRow[0].at("ID"));
    mis::dao::oracle().execute(
        "INSERT INTO users (user_id, username, password_hash, role) "
        "VALUES (:id, :un, :ph, :rl)",
        {{"id", std::to_string(id)}, {"un", username},
         {"ph", passwordHash}, {"rl", role}}
    );
    mis::dao::oracle().commit();
    return id;
}

static std::string oracleGetPasswordHash(const std::string& username)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT password_hash FROM users WHERE username = :un",
        {{"un", username}}
    );
    if (rows.empty()) return {};
    return rows[0].at("PASSWORD_HASH");
}

static UserRecord oracleGetUser(const std::string& username)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT user_id, username, password_hash, role FROM users WHERE username = :un",
        {{"un", username}}
    );
    if (rows.empty()) return {};
    UserRecord u;
    u.userId = std::stoi(rows[0].at("USER_ID"));
    u.username = rows[0].at("USERNAME");
    u.passwordHash = rows[0].at("PASSWORD_HASH");
    u.role = rows[0].at("ROLE");
    return u;
}
#endif

// 内存用户存储（降级模式）
static std::vector<UserRecord> memUsers;
static int memNextUserId = 1;

static int memRegister(const std::string& username, const std::string& passwordHash,
                       const std::string& role)
{
    UserRecord u;
    u.userId = memNextUserId++;
    u.username = username;
    u.passwordHash = passwordHash;
    u.role = role;
    memUsers.push_back(u);
    return u.userId;
}

static std::string memGetPasswordHash(const std::string& username)
{
    for (const auto& u : memUsers) {
        if (u.username == username) return u.passwordHash;
    }
    return {};
}

static UserRecord memGetUser(const std::string& username)
{
    for (const auto& u : memUsers) {
        if (u.username == username) return u;
    }
    return {};
}

// =========================================================================
// 工具函数
// =========================================================================

static std::string ok(const json& data, const std::string& msg = "ok")
{
    return json{{"code", 0}, {"message", msg}, {"data", data}}.dump();
}

static std::string fail(int code, const std::string& msg)
{
    return json{{"code", code}, {"message", msg}}.dump();
}

static bool oracleAvailable()
{
#ifdef MIS_HAS_ORACLE
    try {
        mis::dao::oracle().acquireForCurrentThread();
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        mis::dao::oracle().releaseForCurrentThread();
        return true;
    } catch (...) { return false; }
#else
    return false;
#endif
}

// =========================================================================
// 路由注册
// =========================================================================

void AuthController::registerRoutes(httplib::Server& server)
{
    // ---- 登录 ----
    server.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            if (username.empty() || password.empty()) {
                res.status = 400;
                res.set_content(fail(-1, "用户名和密码不能为空"), "application/json");
                return;
            }

            std::string hash;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                hash = oracleGetPasswordHash(username);
#endif
            } else {
                hash = memGetPasswordHash(username);
            }

            std::string inputHash = mis::utils::Sha256::hashHex(password);
            if (hash.empty() || hash != inputHash) {
                res.status = 401;
                res.set_content(fail(-1, "用户名或密码错误"), "application/json");
                return;
            }

            // 获取用户信息
            UserRecord user;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                user = oracleGetUser(username);
#endif
            } else {
                user = memGetUser(username);
            }

            std::string token = mis::utils::Jwt::sign(user.userId, user.username, user.role);

            json data;
            data["token"] = token;
            data["userId"] = user.userId;
            data["username"] = user.username;
            data["role"] = user.role;

            res.set_content(ok(data, "登录成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // ---- 注册 ----
    server.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            std::string role = body.value("role", "operator");

            if (username.empty() || password.empty()) {
                res.status = 400;
                res.set_content(fail(-1, "用户名和密码不能为空"), "application/json");
                return;
            }
            if (password.size() < 6) {
                res.status = 400;
                res.set_content(fail(-1, "密码长度至少 6 位"), "application/json");
                return;
            }

            std::string passwordHash = mis::utils::Sha256::hashHex(password);
            int userId = 0;

            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                userId = oracleRegister(username, passwordHash, role);
#endif
            } else {
                // 检查重名
                if (!memGetPasswordHash(username).empty()) {
                    res.status = 409;
                    res.set_content(fail(-1, "用户名已存在"), "application/json");
                    return;
                }
                userId = memRegister(username, passwordHash, role);
            }

            std::string token = mis::utils::Jwt::sign(userId, username, role);

            json data;
            data["token"] = token;
            data["userId"] = userId;
            data["username"] = username;
            data["role"] = role;

            res.status = 201;
            res.set_content(ok(data, "注册成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // ---- 登出 ----
    server.Post("/api/auth/logout", [](const httplib::Request&, httplib::Response& res) {
        // 无状态 JWT，客户端自行删除 token
        res.set_content(ok(json::object(), "已登出"), "application/json");
    });

    // ---- 获取当前用户信息 ----
    server.Get("/api/auth/me", [](const httplib::Request&, httplib::Response& res) {
        try {
            json data;
            data["userId"] = mis::context::currentContext.userId;
            data["username"] = mis::context::currentContext.username;
            data["role"] = mis::context::currentContext.role;
            res.set_content(ok(data), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });
}

void registerAuthRoutes(httplib::Server& server)
{
    AuthController{}.registerRoutes(server);
}

} // namespace mis::controllers
