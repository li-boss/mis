#include "controllers/InventoryController.hpp"
#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace mis::controllers {

using json = nlohmann::json;

void InventoryController::registerRoutes(httplib::Server& server)
{
    server.Post("/api/inventory/inbound", [](const httplib::Request& req, httplib::Response& res) {
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
        res.status = 200;
        res.set_content(json{
            {"success", true},
            {"data", {
                {"topItems", json::array()},
                {"trend", json::array()}
            }}
        }.dump(), "application/json");
    });
}

void registerInventoryRoutes(httplib::Server& server)
{
    InventoryController{}.registerRoutes(server);
}

} // namespace mis::controllers
