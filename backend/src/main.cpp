#include "controllers/InventoryController.hpp"
#include "controllers/SkuController.hpp"
#include "controllers/SupplierController.hpp"
#include "utils/Jwt.h"
#include "utils/RequestContext.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"

#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace mis::context {
thread_local RequestContext currentContext;
}

namespace {

std::string envOrDefault(const char* name, const char* fallback)
{
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : value;
}

void applyCors(httplib::Response& res)
{
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

} // namespace

int main()
{
    const auto dbUrl = envOrDefault("MIS_DB_URL", "localhost:1521/XEPDB1");
    const auto dbUser = envOrDefault("MIS_DB_USER", "mis");
    const auto dbPassword = envOrDefault("MIS_DB_PASSWORD", "mis_password");
    const auto port = std::stoi(envOrDefault("MIS_HTTP_PORT", "8080"));

    mis::dao::oracle().initialize(dbUrl, dbUser, dbPassword);

    httplib::Server server;
    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        applyCors(res);
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        // 公开路径：无需鉴权
        if (req.path == "/api/auth/login" || req.path == "/api/auth/register" ||
            req.path == "/api/auth/logout") {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        // 仅对 /api/ 路径进行鉴权
        if (req.path.rfind("/api/", 0) != 0) {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        // 提取 Bearer token
        std::string auth = req.has_header("Authorization")
                            ? req.get_header_value("Authorization") : "";
        if (auth.rfind("Bearer ", 0) != 0) {
            res.status = 401;
            res.set_content(R"({"code":-1,"message":"未登录：缺少 Authorization header"})",
                            "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }

        std::string token = auth.substr(7);

        // 验证 JWT
        try {
            auto claims = mis::utils::Jwt::verify(token);
            // 存入 thread-local 请求上下文以供业务控制器读取
            mis::context::currentContext.userId = claims["userId"].get<int>();
            mis::context::currentContext.username = claims["username"].get<std::string>();
            mis::context::currentContext.role = claims["role"].get<std::string>();
        } catch (const std::exception& ex) {
            res.status = 401;
            std::string body = R"({"code":-1,"message":"token 无效或已过期: )"
                             + std::string(ex.what()) + R"("})";
            res.set_content(body, "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        applyCors(res);
    });

    mis::controllers::registerAuthRoutes(server);
    mis::controllers::registerInventoryRoutes(server);
    mis::controllers::registerSkuRoutes(server);
    mis::controllers::registerSupplierRoutes(server);

    std::cout << "[WMS] 后端已启动 → http://0.0.0.0:" << port << '\n';
    std::cout << "[WMS] API 根路径 → http://localhost:" << port << "/api/inventory\n";
    std::cout << std::flush;

    server.listen("0.0.0.0", port);
    return 0;
}
