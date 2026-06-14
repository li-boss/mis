// =============================================================================
// SkuController — 商品/SKU CRUD
// =============================================================================

#include "controllers/SkuController.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

namespace mis::controllers {

using json = nlohmann::json;

namespace {

struct Sku {
    int id;
    std::string skuCode;
    std::string name;
    std::string category;
    std::string unit;
    std::string supplierName;
    int currentStock;
    int safetyStock;
    std::string status;   // active / disabled
    std::string updatedAt;
};

std::vector<Sku> skus;
std::mutex skuMutex;
int nextSkuId = 2001;

void initDemoSkus()
{
    if (!skus.empty()) return;
    skus.push_back({1001, "SKU-RF-001", "手持扫码终端", "设备", "台", "华东智造供应链", 128, 30, "active", "2026-05-20"});
    skus.push_back({1002, "SKU-PK-018", "标准周转箱",   "耗材", "箱", "青禾包装",         22, 40, "active", "2026-05-21"});
    skus.push_back({1003, "SKU-LB-206", "防水标签纸",   "耗材", "卷", "北辰纸业",        480,120, "active", "2026-05-22"});
    skus.push_back({1004, "SKU-PT-066", "轻型托盘",     "仓储", "个", "启明仓储设备",      0, 20, "disabled","2026-05-16"});
    nextSkuId = 2001;
}

json skuToJson(const Sku& s)
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
    initDemoSkus();

    // 列表
    server.Get("/api/skus", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(skuMutex);
        std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : "";
        std::string category = req.has_param("category") ? req.get_param_value("category") : "";
        std::string status = req.has_param("status") ? req.get_param_value("status") : "";

        std::vector<Sku> filtered;
        for (const auto& s : skus) {
            if (!keyword.empty()) {
                std::string kw = keyword;
                std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
                std::string code = s.skuCode, name = s.name, sup = s.supplierName;
                std::transform(code.begin(), code.end(), code.begin(), ::tolower);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                std::transform(sup.begin(), sup.end(), sup.begin(), ::tolower);
                if (code.find(kw) == std::string::npos &&
                    name.find(kw) == std::string::npos &&
                    sup.find(kw) == std::string::npos) continue;
            }
            if (!category.empty() && s.category != category) continue;
            if (!status.empty() && s.status != status) continue;
            filtered.push_back(s);
        }

        json list = json::array();
        for (const auto& s : filtered) list.push_back(skuToJson(s));

        res.status = 200;
        res.set_content(json{{"code", 0}, {"data", {{"list", list}, {"total", filtered.size()}}}}.dump(), "application/json");
    });

    // 详情
    server.Get(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(skuMutex);
        for (const auto& s : skus) {
            if (s.id == id) {
                res.set_content(json{{"code", 0}, {"data", {{"detail", skuToJson(s)}}}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "SKU not found"}}.dump(), "application/json");
    });

    // 创建
    server.Post("/api/skus", [](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body);
        std::lock_guard<std::mutex> lock(skuMutex);

        Sku s;
        s.id = nextSkuId++;
        s.skuCode = body.value("skuCode", "");
        s.name = body.value("name", "");
        s.category = body.value("category", "耗材");
        s.unit = body.value("unit", "件");
        s.supplierName = body.value("supplierName", "");
        s.currentStock = body.value("currentStock", 0);
        s.safetyStock = body.value("safetyStock", 0);
        s.status = body.value("status", "active");
        s.updatedAt = body.value("updatedAt", "2026-06-14");
        skus.insert(skus.begin(), s);

        res.status = 201;
        res.set_content(json{{"code", 0}, {"message", "SKU created"}, {"data", {{"detail", skuToJson(s)}}}}.dump(), "application/json");
    });

    // 更新
    server.Put(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::lock_guard<std::mutex> lock(skuMutex);

        for (auto& s : skus) {
            if (s.id == id) {
                if (body.contains("skuCode")) s.skuCode = body["skuCode"];
                if (body.contains("name")) s.name = body["name"];
                if (body.contains("category")) s.category = body["category"];
                if (body.contains("unit")) s.unit = body["unit"];
                if (body.contains("supplierName")) s.supplierName = body["supplierName"];
                if (body.contains("currentStock")) s.currentStock = body["currentStock"];
                if (body.contains("safetyStock")) s.safetyStock = body["safetyStock"];
                if (body.contains("status")) s.status = body["status"];
                s.updatedAt = "2026-06-14";
                res.set_content(json{{"code", 0}, {"message", "SKU updated"}, {"data", {{"detail", skuToJson(s)}}}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "SKU not found"}}.dump(), "application/json");
    });

    // 删除
    server.Delete(R"(/api/skus/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(skuMutex);
        auto it = std::remove_if(skus.begin(), skus.end(), [id](const Sku& s) { return s.id == id; });
        if (it != skus.end()) {
            skus.erase(it, skus.end());
            res.set_content(json{{"code", 0}, {"message", "SKU deleted"}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"code", -1}, {"message", "SKU not found"}}.dump(), "application/json");
        }
    });

    // 状态切换
    server.Patch(R"(/api/skus/(\d+)/status)", [](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto body = json::parse(req.body);
        std::string status = body.value("status", "active");
        std::lock_guard<std::mutex> lock(skuMutex);
        for (auto& s : skus) {
            if (s.id == id) {
                s.status = status;
                res.set_content(json{{"code", 0}, {"message", "Status updated"}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(json{{"code", -1}, {"message", "SKU not found"}}.dump(), "application/json");
    });
}

void registerSkuRoutes(httplib::Server& server)
{
    SkuController{}.registerRoutes(server);
}

} // namespace mis::controllers
