// =============================================================================
// SkuController — SKU 商品管理 REST 路由
//
// CRUD 操作：创建、列表、详情、更新、删除。
// Oracle 模式操作 sku 表；内存降级使用 vector 存储。
// =============================================================================

#include "controllers/SkuController.hpp"
#include "utils/RequestContext.hpp"

#include <nlohmann/json.hpp>

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <algorithm>
#include <string>
#include <vector>

namespace mis::controllers {

using json = nlohmann::json;

struct SkuRecord {
    int skuId{0};
    std::string skuCode;
    std::string name;
    std::string category;
    std::string unit;
    double price{0.0};
    bool active{true};
};

// =========================================================================
// Oracle 模式
// =========================================================================
#ifdef MIS_HAS_ORACLE

static int oracleCreateSku(const std::string& code, const std::string& name,
                           const std::string& category, const std::string& unit, double price)
{
    mis::dao::DbSessionGuard db;
    auto seqRow = mis::dao::oracle().query("SELECT seq_sku.NEXTVAL AS ID FROM DUAL");
    int id = std::stoi(seqRow[0].at("ID"));
    mis::dao::oracle().execute(
        "INSERT INTO sku (sku_id, sku_code, name, category, unit, price) "
        "VALUES (:id, :code, :name, :cat, :unit, :price)",
        {{"id", std::to_string(id)}, {"code", code}, {"name", name},
         {"cat", category}, {"unit", unit}, {"price", std::to_string(price)}}
    );
    mis::dao::oracle().commit();
    return id;
}

static std::vector<SkuRecord> oracleListSku(const std::string& keyword, int limit, int offset)
{
    mis::dao::DbSessionGuard db;
    std::string sql = "SELECT sku_id, sku_code, name, category, unit, price, active "
                      "FROM sku WHERE 1=1";
    if (!keyword.empty()) {
        sql += " AND (name LIKE '%" + keyword + "%' OR sku_code LIKE '%" + keyword + "%')";
    }
    sql += " ORDER BY sku_id DESC OFFSET " + std::to_string(offset) +
           " ROWS FETCH NEXT " + std::to_string(limit) + " ROWS ONLY";

    auto rows = mis::dao::oracle().query(sql);
    std::vector<SkuRecord> result;
    for (const auto& r : rows) {
        SkuRecord s;
        s.skuId = std::stoi(r.at("SKU_ID"));
        s.skuCode = r.at("SKU_CODE");
        s.name = r.at("NAME");
        s.category = r.at("CATEGORY");
        s.unit = r.at("UNIT");
        s.price = std::stod(r.at("PRICE"));
        s.active = r.at("ACTIVE") == "1" || r.at("ACTIVE") == "Y";
        result.push_back(s);
    }
    return result;
}

static SkuRecord oracleGetSku(int id)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT sku_id, sku_code, name, category, unit, price, active "
        "FROM sku WHERE sku_id = :id",
        {{"id", std::to_string(id)}}
    );
    if (rows.empty()) return {};
    const auto& r = rows[0];
    SkuRecord s;
    s.skuId = std::stoi(r.at("SKU_ID"));
    s.skuCode = r.at("SKU_CODE");
    s.name = r.at("NAME");
    s.category = r.at("CATEGORY");
    s.unit = r.at("UNIT");
    s.price = std::stod(r.at("PRICE"));
    s.active = r.at("ACTIVE") == "1" || r.at("ACTIVE") == "Y";
    return s;
}

static void oracleUpdateSku(int id, const std::string& name, const std::string& category,
                            const std::string& unit, double price)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "UPDATE sku SET name = :name, category = :cat, unit = :unit, price = :price "
        "WHERE sku_id = :id",
        {{"id", std::to_string(id)}, {"name", name}, {"cat", category},
         {"unit", unit}, {"price", std::to_string(price)}}
    );
    mis::dao::oracle().commit();
}

static void oracleDeleteSku(int id)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "UPDATE sku SET active = 'N' WHERE sku_id = :id",
        {{"id", std::to_string(id)}}
    );
    mis::dao::oracle().commit();
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储（降级模式）
// =========================================================================
static std::vector<SkuRecord> memSkus;
static int memNextSkuId = 1001;

static int memCreateSku(const std::string& code, const std::string& name,
                        const std::string& category, const std::string& unit, double price)
{
    SkuRecord s;
    s.skuId = memNextSkuId++;
    s.skuCode = code;
    s.name = name;
    s.category = category;
    s.unit = unit;
    s.price = price;
    s.active = true;
    memSkus.push_back(s);
    return s.skuId;
}

static std::vector<SkuRecord> memListSku(const std::string& keyword, int limit, int offset)
{
    std::vector<SkuRecord> result;
    for (const auto& s : memSkus) {
        if (!s.active) continue;
        if (!keyword.empty()) {
            if (s.name.find(keyword) == std::string::npos &&
                s.skuCode.find(keyword) == std::string::npos) continue;
        }
        result.push_back(s);
    }
    size_t s = std::min(static_cast<size_t>(offset), result.size());
    size_t e = std::min(s + static_cast<size_t>(limit), result.size());
    return {result.begin() + s, result.begin() + e};
}

static SkuRecord memGetSku(int id)
{
    for (const auto& s : memSkus) {
        if (s.skuId == id && s.active) return s;
    }
    return {};
}

static void memUpdateSku(int id, const std::string& name, const std::string& category,
                         const std::string& unit, double price)
{
    for (auto& s : memSkus) {
        if (s.skuId == id) {
            s.name = name;
            s.category = category;
            s.unit = unit;
            s.price = price;
            return;
        }
    }
}

static void memDeleteSku(int id)
{
    for (auto& s : memSkus) {
        if (s.skuId == id) { s.active = false; return; }
    }
}

// =========================================================================
// Oracle 可用性检测
// =========================================================================
static bool oracleAvailable()
{
#ifdef MIS_HAS_ORACLE
    try {
        mis::dao::oracle().acquireForCurrentThread();
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        mis::dao::oracle().releaseForCurrentThread();
        return true;
    } catch (...) { return false; }
#else
    return false;
#endif
}

// =========================================================================
// 辅助函数
// =========================================================================
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

static json toJson(const SkuRecord& s)
{
    json j;
    j["skuId"] = s.skuId;
    j["skuCode"] = s.skuCode;
    j["name"] = s.name;
    j["category"] = s.category;
    j["unit"] = s.unit;
    j["price"] = s.price;
    j["active"] = s.active;
    return j;
}

// =========================================================================
// 路由注册
// =========================================================================

void SkuController::registerRoutes(httplib::Server& server)
{
    // 1. 创建 SKU  POST /api/sku
    server.Post("/api/sku", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string code = body.value("skuCode", body.value("sku_code", ""));
            std::string name = body.value("name", "");
            std::string category = body.value("category", "");
            std::string unit = body.value("unit", "个");
            double price = body.value("price", 0.0);

            if (name.empty()) {
                res.status = 400;
                res.set_content(fail(-1, "商品名称不能为空"), "application/json");
                return;
            }

            int id;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                id = oracleCreateSku(code, name, category, unit, price);
#endif
            } else {
                id = memCreateSku(code, name, category, unit, price);
            }

            res.status = 201;
            res.set_content(ok({{"skuId", id}}, "SKU 创建成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 2. SKU 列表  GET /api/sku
    server.Get("/api/sku", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string keyword;
            int limit = 50, offset = 0;
            if (req.has_param("keyword")) keyword = req.get_param_value("keyword");
            if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
            if (req.has_param("offset")) offset = std::stoi(req.get_param_value("offset"));

            std::vector<SkuRecord> list;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                list = oracleListSku(keyword, limit, offset);
#endif
            } else {
                list = memListSku(keyword, limit, offset);
            }

            json arr = json::array();
            for (const auto& s : list) arr.push_back(toJson(s));

            res.set_content(ok({{"list", arr}, {"total", static_cast<int>(arr.size())}}),
                            "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 3. SKU 详情  GET /api/sku/:id
    server.Get(R"(/api/sku/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            SkuRecord sku;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                sku = oracleGetSku(id);
#endif
            } else {
                sku = memGetSku(id);
            }
            if (sku.skuId == 0) {
                res.set_content(ok(json(nullptr), "not found"), "application/json");
            } else {
                res.set_content(ok(toJson(sku)), "application/json");
            }
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 4. 更新 SKU  PUT /api/sku/:id
    server.Put(R"(/api/sku/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string category = body.value("category", "");
            std::string unit = body.value("unit", "个");
            double price = body.value("price", 0.0);

            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                oracleUpdateSku(id, name, category, unit, price);
#endif
            } else {
                memUpdateSku(id, name, category, unit, price);
            }

            res.set_content(okMsg("SKU 更新成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 5. 删除 SKU  DELETE /api/sku/:id
    server.Delete(R"(/api/sku/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                oracleDeleteSku(id);
#endif
            } else {
                memDeleteSku(id);
            }
            res.set_content(okMsg("SKU 已删除"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });
}

void registerSkuRoutes(httplib::Server& server)
{
    SkuController{}.registerRoutes(server);
}

} // namespace mis::controllers
