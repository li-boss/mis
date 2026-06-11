#include "controllers/InventoryController.hpp"
#include "dao/OracleConnector.hpp"
#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <exception>

namespace mis::controllers {

using json = nlohmann::json;

void InventoryController::registerRoutes(httplib::Server& server)
{
    server.Post("/api/inventory/inbound", [](const httplib::Request& req, httplib::Response& res) {
        dao::DbSessionGuard dbGuard;
        try {
            const auto body = json::parse(req.body);
            models::InboundRequest inbound{
                body.value("skuId", ""),
                body.value("quantity", 0),
                body.value("warehouseId", "DEFAULT"),
                std::nullopt
            };

            if (body.contains("operatorId") && !body["operatorId"].is_null()) {
                inbound.operatorId = body["operatorId"].get<std::string>();
            }

            services::makeInventoryService().submitInbound(inbound);
            res.status = 200;
            res.set_content(json{{"success", true}, {"message", "Inbound submitted"}}.dump(), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(json{{"success", false}, {"message", ex.what()}}.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"success", false}, {"message", ex.what()}}.dump(), "application/json");
        }
    });

    server.Get("/api/inventory/dashboard", [](const httplib::Request&, httplib::Response& res) {
        dao::DbSessionGuard dbGuard;
        try {
            auto topRows = dao::oracle().query(
                "SELECT sku_id, quantity FROM (SELECT sku_id, quantity FROM Inventory ORDER BY quantity DESC) WHERE ROWNUM <= 5"
            );

            auto trendRows = dao::oracle().query(
                "SELECT TO_CHAR(created_at, 'YYYY-MM-DD') AS day, SUM(quantity) AS qty FROM Inbound_Orders GROUP BY TO_CHAR(created_at, 'YYYY-MM-DD') ORDER BY day ASC"
            );

            json topItems = json::array();
            for (const auto& row : topRows) {
                int quantity = 0;
                try {
                    quantity = std::stoi(row.at("QUANTITY"));
                } catch (...) {}
                topItems.push_back({
                    {"skuId", row.at("SKU_ID")},
                    {"quantity", quantity}
                });
            }

            json trend = json::array();
            for (const auto& row : trendRows) {
                int quantity = 0;
                try {
                    quantity = std::stoi(row.at("QTY"));
                } catch (...) {}
                trend.push_back({
                    {"date", row.at("DAY")},
                    {"quantity", quantity}
                });
            }

            res.status = 200;
            res.set_content(json{
                {"success", true},
                {"data", {
                    {"topItems", topItems},
                    {"trend", trend}
                }}
            }.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"success", false}, {"message", ex.what()}}.dump(), "application/json");
        }
    });
}

void registerInventoryRoutes(httplib::Server& server)
{
    InventoryController{}.registerRoutes(server);
}

} // namespace mis::controllers
