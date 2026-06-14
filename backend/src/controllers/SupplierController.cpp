// =============================================================================
// SupplierController — 供应商 CRUD
// =============================================================================

#include "controllers/SupplierController.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

struct Supplier {
    int id;
    std::string supplierCode;
    std::string name;
    std::string contactName;
    std::string phone;
    std::string rating;    // A / B / C
    std::string status;    // active / paused
    std::string address;
    std::string remark;
};

std::vector<Supplier> suppliers;
std::mutex supMutex;
int nextSupId = 601;

void initDemoSuppliers()
{
    if (!suppliers.empty()) return;
    suppliers.push_back({501, "SUP-HD-001", "华东智造供应链", "陈经理", "13800001234", "A", "active", "上海市浦东新区", "设备类长期合作"});
    suppliers.push_back({502, "SUP-QH-014", "青禾包装",       "刘主管", "13900005678", "B", "active", "苏州市工业园区", "包装耗材月结"});
    suppliers.push_back({503, "SUP-QM-036", "启明仓储设备",   "王工",   "13700007890", "A", "paused", "南京市江宁区", "托盘与货架"});
    nextSupId = 601;
}

json supToJson(const Supplier& s)
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
    initDemoSuppliers();

    // 列表
    server.Get("/api/suppliers", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(supMutex);
        std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : "";
        std::string rating = req.has_param("rating") ? req.get_param_value("rating") : "";
        std::string status = req.has_param("status") ? req.get_param_value("status") : "";

        std::vector<Supplier> filtered;
        for (const auto& s : suppliers) {
            if (!keyword.empty()) {
                std::string kw = keyword;
                std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
                std::string code = s.supplierCode, name = s.name, contact = s.contactName;
                std::transform(code.begin(), code.end(), code.begin(), ::tolower);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                std::transform(contact.begin(), contact.end(), contact.begin(), ::tolower);
                if (code.find(kw) == std::string::npos &&
                    name.find(kw) == std::string::npos &&
                    contact.find(kw) == std::string::npos) continue;
            }
            if (!rating.empty() && s.rating != rating) continue;
            if (!status.empty() && s.status != status) continue;
            filtered.push_back(s);
        }

        json list = json::array();
        for (const auto& s : filtered) list.push_back(supToJson(s));

        res.status = 200;
        res.set_content(json{{"code", 0}, {"data", {{"list", list}, {"total", filtered.size()}}}}.dump(), "application/json");
    });

    // 详情
    server.Get(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(supMutex);
        for (const auto& s : suppliers) {
            if (s.id == id) {
                res.set_content(json{{"code", 0}, {"data", {{"detail", supToJson(s)}}}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "Supplier not found"}}.dump(), "application/json");
    });

    // 创建
    server.Post("/api/suppliers", [](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body);
        std::lock_guard<std::mutex> lock(supMutex);

        Supplier s;
        s.id = nextSupId++;
        s.supplierCode = body.value("supplierCode", "SUP-NEW");
        s.name = body.value("name", "");
        s.contactName = body.value("contactName", "");
        s.phone = body.value("phone", "");
        s.rating = body.value("rating", "B");
        s.status = body.value("status", "active");
        s.address = body.value("address", "");
        s.remark = body.value("remark", "");
        suppliers.insert(suppliers.begin(), s);

        res.status = 201;
        res.set_content(json{{"code", 0}, {"message", "Supplier created"}, {"data", {{"detail", supToJson(s)}}}}.dump(), "application/json");
    });

    // 更新
    server.Put(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::lock_guard<std::mutex> lock(supMutex);
        for (auto& s : suppliers) {
            if (s.id == id) {
                if (body.contains("supplierCode")) s.supplierCode = body["supplierCode"];
                if (body.contains("name")) s.name = body["name"];
                if (body.contains("contactName")) s.contactName = body["contactName"];
                if (body.contains("phone")) s.phone = body["phone"];
                if (body.contains("rating")) s.rating = body["rating"];
                if (body.contains("status")) s.status = body["status"];
                if (body.contains("address")) s.address = body["address"];
                if (body.contains("remark")) s.remark = body["remark"];
                res.set_content(json{{"code", 0}, {"message", "Supplier updated"}, {"data", {{"detail", supToJson(s)}}}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "Supplier not found"}}.dump(), "application/json");
    });

    // 删除
    server.Delete(R"(/api/suppliers/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(supMutex);
        auto it = std::remove_if(suppliers.begin(), suppliers.end(), [id](const Supplier& s) { return s.id == id; });
        if (it != suppliers.end()) {
            suppliers.erase(it, suppliers.end());
            res.set_content(json{{"code", 0}, {"message", "Supplier deleted"}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"code", -1}, {"message", "Supplier not found"}}.dump(), "application/json");
        }
    });

    // 状态切换
    server.Patch(R"(/api/suppliers/(\d+)/status)", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::string status = body.value("status", "active");
        std::lock_guard<std::mutex> lock(supMutex);
        for (auto& s : suppliers) {
            if (s.id == id) {
                s.status = status;
                res.set_content(json{{"code", 0}, {"message", "Status updated"}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "Supplier not found"}}.dump(), "application/json");
    });
}

void registerSupplierRoutes(httplib::Server& server)
{
    SupplierController{}.registerRoutes(server);
}

} // namespace mis::controllers
