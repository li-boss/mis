#include "controllers/InventoryController.hpp"
#include "dao/OracleConnector.hpp"

#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <string>

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
        return httplib::Server::HandlerResponse::Unhandled;
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        applyCors(res);
    });

    mis::controllers::registerInventoryRoutes(server);

    std::cout << "MIS backend listening on http://0.0.0.0:" << port << '\n';
    server.listen("0.0.0.0", port);
    return 0;
}
