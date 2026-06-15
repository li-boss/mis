// =============================================================================
// InventoryController — 入库管理 REST 路由
//
// 当前使用内存存储（独立模式），数据不持久化。
// Oracle 模式：编译时链接 OCI，取消 main.cpp 中 MIS_STANDALONE 注释即可。
// =============================================================================

#include "controllers/InventoryController.hpp"
#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"
#include "services/WarehouseService.hpp"
#include "utils/Encoding.h"

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

            // 解析 lines 数组（采购入库单格式）
            if (body.contains("lines") && body["lines"].is_array()) {
                for (const auto& lj : body["lines"]) {
                    models::InboundLine line;
                    line.productId = lj.value("productId", lj.value("product_id", 0));
                    line.quantityOrdered = lj.value("quantity", lj.value("quantityOrdered", lj.value("quantity_ordered", 0)));
                    line.unitPrice = lj.value("unitPrice", lj.value("unit_price", 0.0));
                    inbound.lines.push_back(line);
                }
            }

            auto result = services::makeInventoryService().createInbound(inbound);
            res.status = 201;
            res.set_content(ok({{"inboundId", result.inboundId}},
                "入库单 " + std::to_string(result.inboundId) + " 创建成功"), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-1, mis::utils::safeError(ex)), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
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
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
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
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
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
            res.set_content(fail(-400, mis::utils::safeError(ex)), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
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
            res.set_content(fail(-300, mis::utils::safeError(ex)), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
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
            res.set_content(fail(-100, mis::utils::safeError(ex)), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
        }
    });

    // 7. 看板数据  GET /api/inventory/dashboard
    server.Get("/api/inventory/dashboard", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // 按仓库过滤
            std::string whCode = req.has_param("warehouseCode")
                ? req.get_param_value("warehouseCode") : "DEFAULT";
            int warehouseId = 1;
            try {
                warehouseId = mis::services::makeWarehouseService().getByCode(whCode).id;
            } catch (...) { warehouseId = 1; }

            auto stats = services::makeInventoryService().getDashboard(warehouseId);

            // 动态趋势数据
            json trend = json::array();
            auto trendData = services::makeInventoryService().getRecentTrend(7, warehouseId);
            for (const auto& tp : trendData) {
                trend.push_back({{"date", tp.date}, {"quantity", tp.quantity}});
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
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
        }
    });

    // 8. SKU 收货（快速入库） POST /api/inventory/inbound/receive-by-sku
    server.Post("/api/inventory/inbound/receive-by-sku", [](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);

            int productId = body.value("productId", body.value("product_id", 0));
            std::string skuCode = body.value("skuCode", body.value("sku_code", ""));
            int quantity = body.value("quantity", 0);

            // 通过 skuCode 解析 productId
            if (productId <= 0 && !skuCode.empty()) {
                // 1. 直接解析为数字（如 "1001"）
                try {
                    productId = std::stoi(skuCode);
                } catch (...) {
                    // 2. 尝试提取 "SKU-数字" 格式（如 "SKU-1001"）
                    std::string s = skuCode;
                    auto dashPos = s.rfind('-');
                    if (dashPos != std::string::npos) {
                        try {
                            productId = std::stoi(s.substr(dashPos + 1));
                        } catch (...) {}
                    }
                    // 3. 仍失败则保持 0
                }
            }

            if (productId <= 0) {
                res.status = 400;
                res.set_content(fail(-1, "无法识别 SKU：" + skuCode
                    + "，请输入数字 productId 或 SKU-数字 格式"), "application/json");
                return;
            }
            if (quantity <= 0) {
                res.status = 400;
                res.set_content(fail(-1, "收货数量必须大于 0"), "application/json");
                return;
            }

            auto result = services::makeInventoryService().receiveBySku(skuCode, productId, quantity);

            res.status = result.success ? 200 : 400;
            json data = {
                {"success", result.success},
                {"totalReceived", result.totalReceived},
                {"orderIds", result.orderIds}
            };
            res.set_content(ok(data, result.message), "application/json");
        } catch (const models::ValidationError& ex) {
            res.status = 400;
            res.set_content(fail(-100, mis::utils::safeError(ex)), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, mis::utils::safeError(ex)), "application/json");
        }
    });

    // 9. 仓库列表  GET /api/inventory/warehouses
    server.Get("/api/inventory/warehouses", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto list = mis::services::makeWarehouseService().list();
            json arr = json::array();
            for (const auto& w : list) {
                arr.push_back({
                    {"id", w.id},
                    {"code", w.code},
                    {"name", w.name},
                    {"address", w.address},
                    {"status", w.status}
                });
            }
            res.status = 200;
            res.set_content(json{{"code", 0}, {"data", {{"list", arr}, {"total", list.size()}}}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });
}

void registerInventoryRoutes(httplib::Server& server)
{
    InventoryController{}.registerRoutes(server);
}

} // namespace mis::controllers
