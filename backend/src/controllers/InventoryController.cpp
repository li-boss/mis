// =============================================================================
// InventoryController — 入库管理 REST 路由
//
// 当前使用内存存储（独立模式），数据不持久化。
// Oracle 模式：编译时链接 OCI，取消 main.cpp 中 MIS_STANDALONE 注释即可。
// =============================================================================

#include "controllers/InventoryController.hpp"
#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <exception>

namespace mis::controllers {

using json = nlohmann::json;

// ---- 辅助函数 ----
static std::string ok(const json& data, const std::string& msg = "ok")
{
    return json{{"code", 0}, {"message", msg}, {"data", data}}.dump();
}

static std::string okMsg(const std::string& msg)
{
    return json{{"code", 0}, {"message", msg}}.dump();
}

static std::string fail(int code, const std::string& msg)
{
    return json{{"code", code}, {"message", msg}}.dump();
}

static json toJson(const models::InboundOrder& order)
{
    json j;
    j["inboundId"] = order.inboundId;
    j["supplierId"] = order.supplierId;
    j["status"] = order.status;
    j["createdBy"] = order.createdBy;
    j["createdAt"] = order.createdAt;
    j["receivedAt"] = order.receivedAt.empty() ? nullptr : json(order.receivedAt);
    j["remark"] = order.remark;

    json linesArr = json::array();
    for (const auto& line : order.lines) {
        json lj;
        lj["lineId"] = line.lineId;
        lj["productId"] = line.productId;
        lj["quantityOrdered"] = line.quantityOrdered;
        lj["quantityReceived"] = line.quantityReceived;
        lj["unitPrice"] = line.unitPrice;
        linesArr.push_back(lj);
    }
    j["lines"] = linesArr;
    return j;
}

// ---- 注册路由 ----
void InventoryController::registerRoutes(httplib::Server& server)
{
    // 1. 创建入库单  POST /api/inventory/inbound
    server.Post("/api/inventory/inbound", [](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);

            models::InboundRequest inbound;
            inbound.supplierId = body.value("supplierId", body.value("supplier_id", 0));
            inbound.createdBy = body.value("createdBy", body.value("created_by", 0));
            inbound.remark = body.value("remark", "");

            // 解析 lines 数组
            if (body.contains("lines") && body["lines"].is_array()) {
                for (const auto& lj : body["lines"]) {
                    models::InboundLine line;
                    line.productId = lj.value("productId", lj.value("product_id", 0));
                    line.quantityOrdered = lj.value("quantity", lj.value("quantityOrdered", lj.value("quantity_ordered", 0)));
                    line.unitPrice = lj.value("unitPrice", lj.value("unit_price", 0.0));
                    inbound.lines.push_back(line);
                }
            }

            // 兼容 Dashboard 快捷入库（单 SKU）
            if (inbound.lines.empty() && body.contains("skuCode")) {
                models::InboundLine line;
                std::string skuStr = body.value("skuCode", "0");
                // 尝试解析数字 ID
                try { line.productId = std::stoi(skuStr); }
                catch (...) { line.productId = std::hash<std::string>{}(skuStr) % 9000 + 1000; }
                line.quantityOrdered = body.value("quantity", 0);
                line.unitPrice = 0.0;
                inbound.lines.push_back(line);
            }

            // 未传 supplierId 给默认值
            if (inbound.supplierId <= 0) {
                inbound.supplierId = body.value("supplierId", body.value("supplier_id", 1));
            }

            auto result = services::makeInventoryService().createInbound(inbound);
            res.status = 201;
            res.set_content(ok({{"inboundId", result.inboundId}},
                "入库单 " + std::to_string(result.inboundId) + " 创建成功"), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-1, ex.what()), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 2. 入库单列表  GET /api/inventory/inbound
    server.Get("/api/inventory/inbound", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string status;
            int supplierId = 0, limit = 50, offset = 0;

            if (req.has_param("status"))       status = req.get_param_value("status");
            if (req.has_param("supplierId"))   supplierId = std::stoi(req.get_param_value("supplierId"));
            if (req.has_param("supplier_id"))  supplierId = std::stoi(req.get_param_value("supplier_id"));
            if (req.has_param("limit"))        limit = std::stoi(req.get_param_value("limit"));
            if (req.has_param("offset"))       offset = std::stoi(req.get_param_value("offset"));

            auto orders = services::makeInventoryService().listInbound(status, supplierId, limit, offset);

            json dataArr = json::array();
            for (const auto& o : orders) {
                dataArr.push_back(toJson(o));
            }

            // 总计数（简化：传 0 查全部）
            int total = static_cast<int>(services::makeInventoryService().listInbound(status, supplierId, 9999, 0).size());

            res.status = 200;
            res.set_content(ok({{"list", dataArr}, {"total", total}}), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 3. 入库单详情  GET /api/inventory/inbound/:id
    server.Get(R"(/api/inventory/inbound/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto order = services::makeInventoryService().getInbound(id);
            if (order.inboundId == 0) {
                res.status = 200;
                res.set_content(ok(json(nullptr), "not found"), "application/json");
            } else {
                res.status = 200;
                res.set_content(ok(toJson(order)), "application/json");
            }
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 4. 提交入库单  POST /api/inventory/inbound/:id/submit
    server.Post(R"(/api/inventory/inbound/(\d+)/submit)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            services::makeInventoryService().submitInbound(id, 0);
            res.status = 200;
            res.set_content(okMsg("入库单 " + std::to_string(id) + " 已提交"), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-400, ex.what()), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 5. 取消入库单  POST /api/inventory/inbound/:id/cancel
    server.Post(R"(/api/inventory/inbound/(\d+)/cancel)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            services::makeInventoryService().cancelInbound(id, 0);
            res.status = 200;
            res.set_content(okMsg("入库单 " + std::to_string(id) + " 已取消"), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-300, ex.what()), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 6. 收货确认  POST /api/inventory/inbound/lines/:line_id/receive
    server.Post(R"(/api/inventory/inbound/lines/(\d+)/receive)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int lineId = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            double quantity = body.value("receiveQuantity", body.value("receive_quantity", 0.0));

            services::makeInventoryService().receiveLine(lineId, quantity, 0);
            res.status = 200;
            res.set_content(ok(json::object(), "收货成功"), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-100, ex.what()), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 7. 看板数据  GET /api/inventory/dashboard
    server.Get("/api/inventory/dashboard", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto stats = services::makeInventoryService().getDashboard();

            // 模拟趋势数据
            json trend = json::array();
            const char* days[] = {"06-08","06-09","06-10","06-11","06-12","06-13","06-14"};
            int values[] = {128, 184, 146, 236, 211, 274, 336};
            for (int i = 0; i < 7; ++i) {
                trend.push_back({{"date", days[i]}, {"quantity", values[i]}});
            }

            res.status = 200;
            res.set_content(ok(json{
                {"totalStock", stats.totalStock},
                {"inboundToday", stats.inboundToday},
                {"lowStockSku", stats.lowStockSku},
                {"exceptionCount", stats.exceptionCount},
                {"pendingInbound", stats.pendingInbound},
                {"trend", trend}
            }), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });
}

void registerInventoryRoutes(httplib::Server& server)
{
    InventoryController{}.registerRoutes(server);
}

} // namespace mis::controllers
