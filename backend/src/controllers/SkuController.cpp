// =============================================================================
// SkuController — 商品/SKU CRUD
// Oracle 优先 + 内存降级（通过 SkuService）
// =============================================================================

#include "controllers/SkuController.hpp"
#include "services/SkuService.hpp"
#include "services/WarehouseService.hpp"
#include "utils/Encoding.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

json skuToJson(const mis::services::SkuService::SkuItem& s)
{
    return {
        {"id", s.id},
        {"skuCode", s.skuCode},
        {"name", s.name},
        {"category", s.category},
        {"unit", s.unit},
        {"supplierName", s.supplierName},
        {"currentStock", s.currentStock},
        {"safetyStock", s.safetyStock},
        {"status", s.status},
        {"updatedAt", s.updatedAt}
    };
}

} // namespace

void SkuController::registerRoutes(httplib::Server& server)
{
    // 解析仓库参数
    auto resolveWh = [](const httplib::Request& req) -> int {
        std::string whCode = req.has_param("warehouseCode")
            ? req.get_param_value("warehouseCode") : "DEFAULT";
        try { return mis::services::makeWarehouseService().getByCode(whCode).id; }
        catch (...) { return 1; }
    };

    // 列表
    server.Get("/api/skus", [&resolveWh](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : "";
            std::string category = req.has_param("category") ? req.get_param_value("category") : "";
            std::string status = req.has_param("status") ? req.get_param_value("status") : "";
            bool lowStock = req.has_param("lowStock") && req.get_param_value("lowStock") == "1";

            int whId = resolveWh(req);
            auto list = mis::services::makeSkuService().list(keyword, category, status, whId, lowStock);
            json arr = json::array();
            for (const auto& s : list) arr.push_back(skuToJson(s));

            res.status = 200;
            res.set_content(json{{"code", 0}, {"data", {{"list", arr}, {"total", list.size()}}}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 详情
    server.Get(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto s = mis::services::makeSkuService().getById(id);
            res.set_content(json{{"code", 0}, {"data", {{"detail", skuToJson(s)}}}}.dump(),
                            "application/json");
        } catch (const std::runtime_error& ex) {
            res.status = 404;
            res.set_content(json{{"code", -1}, {"message", mis::utils::safeError(ex)}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 创建
    server.Post("/api/skus", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            mis::services::SkuService::SkuItem item;
            item.skuCode = body.value("skuCode", "");
            item.name = body.value("name", "");
            item.category = body.value("category", "耗材");
            item.unit = body.value("unit", "件");
            item.currentStock = body.value("currentStock", 0);
            item.safetyStock = body.value("safetyStock", 0);
            item.status = body.value("status", "active");

            auto created = mis::services::makeSkuService().create(item);
            res.status = 201;
            res.set_content(json{{"code", 0}, {"message", "SKU created"},
                {"data", {{"detail", skuToJson(created)}}}}.dump(), "application/json");
        } catch (const json::parse_error& ex) {
            res.status = 400;
            res.set_content(json{{"code", -98},
                {"message", std::string("请求数据格式错误: ") + mis::utils::safeError(ex)}}.dump(),
                "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 更新
    server.Put(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            mis::services::SkuService::SkuItem item;
            if (body.contains("skuCode"))      item.skuCode = body["skuCode"];
            if (body.contains("name"))         item.name = body["name"];
            if (body.contains("category"))     item.category = body["category"];
            if (body.contains("unit"))         item.unit = body["unit"];
            if (body.contains("currentStock")) item.currentStock = body["currentStock"];
            if (body.contains("safetyStock"))  item.safetyStock = body["safetyStock"];
            if (body.contains("status"))       item.status = body["status"];

            auto updated = mis::services::makeSkuService().update(id, item);
            res.set_content(json{{"code", 0}, {"message", "SKU updated"},
                {"data", {{"detail", skuToJson(updated)}}}}.dump(), "application/json");
        } catch (const std::runtime_error& ex) {
            res.status = 404;
            res.set_content(json{{"code", -1}, {"message", mis::utils::safeError(ex)}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 删除
    server.Delete(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            mis::services::makeSkuService().remove(id);
            res.set_content(json{{"code", 0}, {"message", "SKU deleted"}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 状态切换
    server.Patch(R"(/api/skus/(\d+)/status)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            std::string status = body.value("status", "active");
            mis::services::makeSkuService().updateStatus(id, status);
            res.set_content(json{{"code", 0}, {"message", "Status updated"}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });
}

void registerSkuRoutes(httplib::Server& server)
{
    SkuController{}.registerRoutes(server);
}

} // namespace mis::controllers
