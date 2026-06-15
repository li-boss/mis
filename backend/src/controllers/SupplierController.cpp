// =============================================================================
// SupplierController — 供应商 CRUD
// Oracle 优先 + 内存降级（通过 SupplierService）
// =============================================================================

#include "controllers/SupplierController.hpp"
#include "services/SupplierService.hpp"
#include "utils/Encoding.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

json supToJson(const mis::services::SupplierService::SupplierItem& s)
{
    return {
        {"id", s.id},
        {"supplierCode", s.supplierCode},
        {"name", s.name},
        {"contactName", s.contactName},
        {"phone", s.phone},
        {"rating", s.rating},
        {"status", s.status},
        {"address", s.address},
        {"remark", s.remark}
    };
}

} // namespace

void SupplierController::registerRoutes(httplib::Server& server)
{
    // 列表
    server.Get("/api/suppliers", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string keyword  = req.has_param("keyword")  ? req.get_param_value("keyword")  : "";
            std::string rating   = req.has_param("rating")   ? req.get_param_value("rating")   : "";
            std::string status   = req.has_param("status")   ? req.get_param_value("status")   : "";

            auto list = mis::services::makeSupplierService().list(keyword, rating, status);
            json arr = json::array();
            for (const auto& s : list) arr.push_back(supToJson(s));

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
    server.Get(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto s = mis::services::makeSupplierService().getById(id);
            res.set_content(json{{"code", 0}, {"data", {{"detail", supToJson(s)}}}}.dump(),
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
    server.Post("/api/suppliers", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            mis::services::SupplierService::SupplierItem item;
            item.supplierCode = body.value("supplierCode", "SUP-NEW");
            item.name = body.value("name", "");
            item.contactName = body.value("contactName", "");
            item.phone = body.value("phone", "");
            item.rating = body.value("rating", "B");
            item.status = body.value("status", "active");
            item.address = body.value("address", "");
            item.remark = body.value("remark", "");

            auto created = mis::services::makeSupplierService().create(item);
            res.status = 201;
            res.set_content(json{{"code", 0}, {"message", "Supplier created"},
                {"data", {{"detail", supToJson(created)}}}}.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 更新
    server.Put(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            mis::services::SupplierService::SupplierItem item;
            if (body.contains("supplierCode")) item.supplierCode = body["supplierCode"];
            if (body.contains("name"))         item.name = body["name"];
            if (body.contains("contactName"))  item.contactName = body["contactName"];
            if (body.contains("phone"))        item.phone = body["phone"];
            if (body.contains("rating"))       item.rating = body["rating"];
            if (body.contains("status"))       item.status = body["status"];
            if (body.contains("address"))      item.address = body["address"];
            if (body.contains("remark"))       item.remark = body["remark"];

            auto updated = mis::services::makeSupplierService().update(id, item);
            res.set_content(json{{"code", 0}, {"message", "Supplier updated"},
                {"data", {{"detail", supToJson(updated)}}}}.dump(), "application/json");
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
    server.Delete(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            mis::services::makeSupplierService().remove(id);
            res.set_content(json{{"code", 0}, {"message", "Supplier deleted"}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });

    // 状态切换
    server.Patch(R"(/api/suppliers/(\d+)/status)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            std::string status = body.value("status", "active");
            mis::services::makeSupplierService().updateStatus(id, status);
            res.set_content(json{{"code", 0}, {"message", "Status updated"}}.dump(),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"code", -99},
                {"message", mis::utils::safeError(ex)}}.dump(), "application/json");
        }
    });
}

void registerSupplierRoutes(httplib::Server& server)
{
    SupplierController{}.registerRoutes(server);
}

} // namespace mis::controllers
