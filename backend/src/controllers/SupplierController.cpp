// =============================================================================
// SupplierController — 供应商管理 REST 路由
//
// CRUD 操作：创建、列表、详情、更新、删除。
// Oracle 模式操作 suppliers 表；内存降级使用 vector 存储。
// =============================================================================

#include "controllers/SupplierController.hpp"
#include "utils/RequestContext.hpp"

#include <nlohmann/json.hpp>

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <string>
#include <vector>

namespace mis::controllers {

using json = nlohmann::json;

struct SupplierRecord {
    int supplierId{0};
    std::string name;
    std::string contactPerson;
    std::string phone;
    std::string email;
    std::string address;
    bool active{true};
};

// =========================================================================
// Oracle 模式
// =========================================================================
#ifdef MIS_HAS_ORACLE

static int oracleCreateSupplier(const std::string& name, const std::string& contact,
                                const std::string& phone, const std::string& email,
                                const std::string& address)
{
    mis::dao::DbSessionGuard db;
    auto seqRow = mis::dao::oracle().query("SELECT seq_suppliers.NEXTVAL AS ID FROM DUAL");
    int id = std::stoi(seqRow[0].at("ID"));
    mis::dao::oracle().execute(
        "INSERT INTO suppliers (supplier_id, name, contact_person, phone, email, address) "
        "VALUES (:id, :name, :contact, :phone, :email, :addr)",
        {{"id", std::to_string(id)}, {"name", name}, {"contact", contact},
         {"phone", phone}, {"email", email}, {"addr", address}}
    );
    mis::dao::oracle().commit();
    return id;
}

static std::vector<SupplierRecord> oracleListSupplier(const std::string& keyword,
                                                       int limit, int offset)
{
    mis::dao::DbSessionGuard db;
    std::string sql = "SELECT supplier_id, name, contact_person, phone, email, address, active "
                      "FROM suppliers WHERE 1=1";
    if (!keyword.empty()) {
        sql += " AND (name LIKE '%" + keyword + "%' OR contact_person LIKE '%" + keyword + "%')";
    }
    sql += " ORDER BY supplier_id DESC OFFSET " + std::to_string(offset) +
           " ROWS FETCH NEXT " + std::to_string(limit) + " ROWS ONLY";

    auto rows = mis::dao::oracle().query(sql);
    std::vector<SupplierRecord> result;
    for (const auto& r : rows) {
        SupplierRecord s;
        s.supplierId = std::stoi(r.at("SUPPLIER_ID"));
        s.name = r.at("NAME");
        s.contactPerson = r.at("CONTACT_PERSON");
        s.phone = r.at("PHONE");
        s.email = r.at("EMAIL");
        s.address = r.at("ADDRESS");
        s.active = r.at("ACTIVE") == "1" || r.at("ACTIVE") == "Y";
        result.push_back(s);
    }
    return result;
}

static SupplierRecord oracleGetSupplier(int id)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT supplier_id, name, contact_person, phone, email, address, active "
        "FROM suppliers WHERE supplier_id = :id",
        {{"id", std::to_string(id)}}
    );
    if (rows.empty()) return {};
    const auto& r = rows[0];
    SupplierRecord s;
    s.supplierId = std::stoi(r.at("SUPPLIER_ID"));
    s.name = r.at("NAME");
    s.contactPerson = r.at("CONTACT_PERSON");
    s.phone = r.at("PHONE");
    s.email = r.at("EMAIL");
    s.address = r.at("ADDRESS");
    s.active = r.at("ACTIVE") == "1" || r.at("ACTIVE") == "Y";
    return s;
}

static void oracleUpdateSupplier(int id, const std::string& name, const std::string& contact,
                                 const std::string& phone, const std::string& email,
                                 const std::string& address)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "UPDATE suppliers SET name = :name, contact_person = :contact, "
        "phone = :phone, email = :email, address = :addr WHERE supplier_id = :id",
        {{"id", std::to_string(id)}, {"name", name}, {"contact", contact},
         {"phone", phone}, {"email", email}, {"addr", address}}
    );
    mis::dao::oracle().commit();
}

static void oracleDeleteSupplier(int id)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "UPDATE suppliers SET active = 'N' WHERE supplier_id = :id",
        {{"id", std::to_string(id)}}
    );
    mis::dao::oracle().commit();
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储（降级模式）
// =========================================================================
static std::vector<SupplierRecord> memSuppliers;
static int memNextSupplierId = 1;

static int memCreateSupplier(const std::string& name, const std::string& contact,
                             const std::string& phone, const std::string& email,
                             const std::string& address)
{
    SupplierRecord s;
    s.supplierId = memNextSupplierId++;
    s.name = name;
    s.contactPerson = contact;
    s.phone = phone;
    s.email = email;
    s.address = address;
    s.active = true;
    memSuppliers.push_back(s);
    return s.supplierId;
}

static std::vector<SupplierRecord> memListSupplier(const std::string& keyword,
                                                    int limit, int offset)
{
    std::vector<SupplierRecord> result;
    for (const auto& s : memSuppliers) {
        if (!s.active) continue;
        if (!keyword.empty()) {
            if (s.name.find(keyword) == std::string::npos &&
                s.contactPerson.find(keyword) == std::string::npos) continue;
        }
        result.push_back(s);
    }
    size_t si = std::min(static_cast<size_t>(offset), result.size());
    size_t e = std::min(si + static_cast<size_t>(limit), result.size());
    return {result.begin() + si, result.begin() + e};
}

static SupplierRecord memGetSupplier(int id)
{
    for (const auto& s : memSuppliers) {
        if (s.supplierId == id && s.active) return s;
    }
    return {};
}

static void memUpdateSupplier(int id, const std::string& name, const std::string& contact,
                              const std::string& phone, const std::string& email,
                              const std::string& address)
{
    for (auto& s : memSuppliers) {
        if (s.supplierId == id) {
            s.name = name;
            s.contactPerson = contact;
            s.phone = phone;
            s.email = email;
            s.address = address;
            return;
        }
    }
}

static void memDeleteSupplier(int id)
{
    for (auto& s : memSuppliers) {
        if (s.supplierId == id) { s.active = false; return; }
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

static json toJson(const SupplierRecord& s)
{
    json j;
    j["supplierId"] = s.supplierId;
    j["name"] = s.name;
    j["contactPerson"] = s.contactPerson;
    j["phone"] = s.phone;
    j["email"] = s.email;
    j["address"] = s.address;
    j["active"] = s.active;
    return j;
}

// =========================================================================
// 路由注册
// =========================================================================

void SupplierController::registerRoutes(httplib::Server& server)
{
    // 1. 创建供应商  POST /api/supplier
    server.Post("/api/supplier", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string contact = body.value("contactPerson", body.value("contact_person", ""));
            std::string phone = body.value("phone", "");
            std::string email = body.value("email", "");
            std::string address = body.value("address", "");

            if (name.empty()) {
                res.status = 400;
                res.set_content(fail(-1, "供应商名称不能为空"), "application/json");
                return;
            }

            int id;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                id = oracleCreateSupplier(name, contact, phone, email, address);
#endif
            } else {
                id = memCreateSupplier(name, contact, phone, email, address);
            }

            res.status = 201;
            res.set_content(ok({{"supplierId", id}}, "供应商创建成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 2. 供应商列表  GET /api/supplier
    server.Get("/api/supplier", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string keyword;
            int limit = 50, offset = 0;
            if (req.has_param("keyword")) keyword = req.get_param_value("keyword");
            if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
            if (req.has_param("offset")) offset = std::stoi(req.get_param_value("offset"));

            std::vector<SupplierRecord> list;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                list = oracleListSupplier(keyword, limit, offset);
#endif
            } else {
                list = memListSupplier(keyword, limit, offset);
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

    // 3. 供应商详情  GET /api/supplier/:id
    server.Get(R"(/api/supplier/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            SupplierRecord s;
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                s = oracleGetSupplier(id);
#endif
            } else {
                s = memGetSupplier(id);
            }
            if (s.supplierId == 0) {
                res.set_content(ok(json(nullptr), "not found"), "application/json");
            } else {
                res.set_content(ok(toJson(s)), "application/json");
            }
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 4. 更新供应商  PUT /api/supplier/:id
    server.Put(R"(/api/supplier/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string contact = body.value("contactPerson", body.value("contact_person", ""));
            std::string phone = body.value("phone", "");
            std::string email = body.value("email", "");
            std::string address = body.value("address", "");

            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                oracleUpdateSupplier(id, name, contact, phone, email, address);
#endif
            } else {
                memUpdateSupplier(id, name, contact, phone, email, address);
            }

            res.set_content(okMsg("供应商更新成功"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });

    // 5. 删除供应商  DELETE /api/supplier/:id
    server.Delete(R"(/api/supplier/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            if (oracleAvailable()) {
#ifdef MIS_HAS_ORACLE
                oracleDeleteSupplier(id);
#endif
            } else {
                memDeleteSupplier(id);
            }
            res.set_content(okMsg("供应商已删除"), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(fail(-99, ex.what()), "application/json");
        }
    });
}

void registerSupplierRoutes(httplib::Server& server)
{
    SupplierController{}.registerRoutes(server);
}

} // namespace mis::controllers
