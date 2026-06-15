// =============================================================================
// SupplierService — 供应商管理
// Oracle 优先 + 内存降级
// =============================================================================

#include "services/SupplierService.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <algorithm>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace mis::services {

static SupplierService::SupplierItem rowToSupplier(
    const std::unordered_map<std::string, std::string>& r)
{
    SupplierService::SupplierItem s;
    s.id = std::stoi(r.at("SUPPLIER_ID"));
    s.name = r.at("SUPPLIER_NAME");
    s.supplierCode = r.count("SUPPLIER_CODE") ? r.at("SUPPLIER_CODE") : "";
    s.contactName = r.count("CONTACT_NAME") ? r.at("CONTACT_NAME") : "";
    s.phone = r.count("CONTACT_PHONE") ? r.at("CONTACT_PHONE") : "";
    s.rating = r.count("RATING") ? r.at("RATING") : "B";
    s.status = r.count("STATUS") ? r.at("STATUS") : "active";
    s.address = r.count("ADDRESS") ? r.at("ADDRESS") : "";
    s.remark = r.count("REMARK") ? r.at("REMARK") : "";
    return s;
}

static bool oracleAvailable()
{
#ifdef MIS_HAS_ORACLE
    static bool checked = false;
    static bool available = false;
    if (checked) return available;
    checked = true;
    try {
        mis::dao::DbSessionGuard db;
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        available = true;
        std::cout << "[SUPPLIER] Oracle 检测通过，使用 Oracle 存储\n";
    } catch (...) {
        std::cerr << "[SUPPLIER] Oracle 不可用，降级内存存储\n";
        available = false;
    }
    return available;
#else
    return false;
#endif
}

// =========================================================================
// Oracle 模式
// =========================================================================
#ifdef MIS_HAS_ORACLE

static std::vector<SupplierService::SupplierItem> oracleList(
    const std::string& keyword, const std::string& rating, const std::string& status)
{
    mis::dao::DbSessionGuard db;
    std::string sql = "SELECT * FROM suppliers WHERE 1=1";
    std::unordered_map<std::string, std::string> binds;
    if (!keyword.empty()) {
        sql += " AND (UPPER(supplier_name) LIKE UPPER(:kw) OR UPPER(supplier_code) LIKE UPPER(:kw2))";
        binds["kw"] = "%" + keyword + "%";
        binds["kw2"] = "%" + keyword + "%";
    }
    if (!rating.empty()) { sql += " AND rating = :rt"; binds["rt"] = rating; }
    if (!status.empty()) { sql += " AND status = :st"; binds["st"] = status; }
    sql += " ORDER BY supplier_id";

    auto rows = mis::dao::oracle().query(sql, binds);
    std::vector<SupplierService::SupplierItem> result;
    for (const auto& r : rows) result.push_back(rowToSupplier(r));
    return result;
}

static SupplierService::SupplierItem oracleGetById(int id)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT * FROM suppliers WHERE supplier_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty()) throw std::runtime_error("Supplier not found");
    return rowToSupplier(rows[0]);
}

static SupplierService::SupplierItem oracleCreate(const SupplierService::SupplierItem& item)
{
    mis::dao::DbSessionGuard db;
    auto seq = mis::dao::oracle().query("SELECT seq_suppliers.NEXTVAL AS ID FROM DUAL");
    int newId = std::stoi(seq[0].at("ID"));

    mis::dao::oracle().execute(
        "INSERT INTO suppliers (supplier_id, supplier_name, supplier_code, contact_name, "
        "contact_phone, rating, status, address, remark) "
        "VALUES (:id, :name, :code, :cname, :phone, :rating, :status, :addr, :rmk)",
        {{"id", std::to_string(newId)}, {"name", item.name},
         {"code", item.supplierCode.empty() ? "SUP-NEW" : item.supplierCode},
         {"cname", item.contactName}, {"phone", item.phone},
         {"rating", item.rating.empty() ? "B" : item.rating},
         {"status", item.status.empty() ? "active" : item.status},
         {"addr", item.address}, {"rmk", item.remark}}
    );
    mis::dao::oracle().commit();
    return oracleGetById(newId);
}

static SupplierService::SupplierItem oracleUpdate(int id, const SupplierService::SupplierItem& item)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM suppliers WHERE supplier_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty() || std::stoi(rows[0].at("CNT")) == 0)
        throw std::runtime_error("Supplier not found");

    std::string sql = "UPDATE suppliers SET ";
    std::unordered_map<std::string, std::string> binds;
    bool first = true;
    auto add = [&](const std::string& col, const std::string& val) {
        if (!first) sql += ", ";
        sql += col + " = :" + col;
        binds[col] = val;
        first = false;
    };
    if (!item.name.empty()) add("supplier_name", item.name);
    if (!item.supplierCode.empty()) add("supplier_code", item.supplierCode);
    if (!item.contactName.empty()) add("contact_name", item.contactName);
    if (!item.phone.empty()) add("contact_phone", item.phone);
    if (!item.rating.empty()) add("rating", item.rating);
    if (!item.status.empty()) add("status", item.status);
    // address 和 remark 始终更新（允许清空）
    add("address", item.address);
    add("remark", item.remark);

    if (!first) {
        sql += " WHERE supplier_id = :pid";
        binds["pid"] = std::to_string(id);
        mis::dao::oracle().execute(sql, binds);
    }
    mis::dao::oracle().commit();
    return oracleGetById(id);
}

static void oracleRemove(int id)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "DELETE FROM suppliers WHERE supplier_id = :id",
        {{"id", std::to_string(id)}});
    mis::dao::oracle().commit();
}

static void oracleUpdateStatus(int id, const std::string& status)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM suppliers WHERE supplier_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty() || std::stoi(rows[0].at("CNT")) == 0)
        throw std::runtime_error("Supplier not found");
    mis::dao::oracle().execute(
        "UPDATE suppliers SET status = :st WHERE supplier_id = :id",
        {{"st", status}, {"id", std::to_string(id)}});
    mis::dao::oracle().commit();
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储
// =========================================================================

namespace {

struct MemSupplier {
    int id; std::string supplierCode, name, contactName, phone;
    std::string rating, status, address, remark;
};

std::vector<MemSupplier> memSuppliers;
std::mutex memMutex;
int memNextId = 601;

void initMemSuppliers()
{
    if (!memSuppliers.empty()) return;
    memSuppliers.push_back({501, "SUP-HD-001", "华东智造供应链", "陈经理", "13800001234", "A", "active", "上海市浦东新区", "设备类长期合作"});
    memSuppliers.push_back({502, "SUP-QH-014", "青禾包装", "刘主管", "13900005678", "B", "active", "苏州市工业园区", "包装耗材月结"});
    memSuppliers.push_back({503, "SUP-QM-036", "启明仓储设备", "王工", "13700007890", "A", "paused", "南京市江宁区", "托盘与货架"});
    memNextId = 601;
}

SupplierService::SupplierItem memToSupplier(const MemSupplier& m)
{
    SupplierService::SupplierItem s;
    s.id = m.id; s.supplierCode = m.supplierCode; s.name = m.name;
    s.contactName = m.contactName; s.phone = m.phone;
    s.rating = m.rating; s.status = m.status;
    s.address = m.address; s.remark = m.remark;
    return s;
}

} // namespace

static std::vector<SupplierService::SupplierItem> memList(
    const std::string& keyword, const std::string& rating, const std::string& status)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSuppliers();
    std::vector<SupplierService::SupplierItem> result;
    for (const auto& s : memSuppliers) {
        if (!keyword.empty()) {
            std::string kw = keyword;
            std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
            std::string name = s.name, code = s.supplierCode;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::transform(code.begin(), code.end(), code.begin(), ::tolower);
            if (name.find(kw) == std::string::npos && code.find(kw) == std::string::npos) continue;
        }
        if (!rating.empty() && s.rating != rating) continue;
        if (!status.empty() && s.status != status) continue;
        result.push_back(memToSupplier(s));
    }
    return result;
}

static SupplierService::SupplierItem memGetById(int id) {
    std::lock_guard<std::mutex> lock(memMutex); initMemSuppliers();
    for (const auto& s : memSuppliers) if (s.id == id) return memToSupplier(s);
    throw std::runtime_error("Supplier not found");
}

static SupplierService::SupplierItem memCreate(const SupplierService::SupplierItem& item) {
    std::lock_guard<std::mutex> lock(memMutex); initMemSuppliers();
    MemSupplier m;
    m.id = memNextId++;
    m.supplierCode = item.supplierCode.empty() ? "SUP-NEW" : item.supplierCode;
    m.name = item.name;
    m.contactName = item.contactName; m.phone = item.phone;
    m.rating = item.rating.empty() ? "B" : item.rating;
    m.status = item.status.empty() ? "active" : item.status;
    m.address = item.address; m.remark = item.remark;
    memSuppliers.insert(memSuppliers.begin(), m);
    return memToSupplier(m);
}

static SupplierService::SupplierItem memUpdate(int id, const SupplierService::SupplierItem& item) {
    std::lock_guard<std::mutex> lock(memMutex); initMemSuppliers();
    for (auto& s : memSuppliers) {
        if (s.id != id) continue;
        if (!item.supplierCode.empty()) s.supplierCode = item.supplierCode;
        if (!item.name.empty()) s.name = item.name;
        if (!item.contactName.empty()) s.contactName = item.contactName;
        if (!item.phone.empty()) s.phone = item.phone;
        if (!item.rating.empty()) s.rating = item.rating;
        if (!item.status.empty()) s.status = item.status;
        s.address = item.address;
        s.remark = item.remark;
        return memToSupplier(s);
    }
    throw std::runtime_error("Supplier not found");
}

static void memRemove(int id) {
    std::lock_guard<std::mutex> lock(memMutex); initMemSuppliers();
    auto it = std::remove_if(memSuppliers.begin(), memSuppliers.end(),
        [id](const MemSupplier& s) { return s.id == id; });
    if (it != memSuppliers.end()) memSuppliers.erase(it, memSuppliers.end());
}

static void memUpdateStatus(int id, const std::string& status) {
    std::lock_guard<std::mutex> lock(memMutex); initMemSuppliers();
    for (auto& s : memSuppliers) { if (s.id == id) { s.status = status; return; } }
    throw std::runtime_error("Supplier not found");
}

// =========================================================================
// 统一接口
// =========================================================================

#ifdef MIS_HAS_ORACLE
#define TRY_SUP_ORACLE(call, fallback) \
    do { if (oracleAvailable()) { try { return oracle##call; } catch (const std::exception& ex) { \
        std::cerr << "[SUPPLIER] Oracle 操作失败: " << ex.what() << "，降级内存\n"; } } return mem##call; } while(0)
#define TRY_SUP_ORACLE_VOID(call, fallback) \
    do { if (oracleAvailable()) { try { oracle##call; return; } catch (const std::exception& ex) { \
        std::cerr << "[SUPPLIER] Oracle 操作失败: " << ex.what() << "，降级内存\n"; } } mem##call; } while(0)
#else
#define TRY_SUP_ORACLE(call, fallback) return mem##call;
#define TRY_SUP_ORACLE_VOID(call, fallback) mem##call;
#endif

std::vector<SupplierService::SupplierItem> SupplierService::list(
    const std::string& kw, const std::string& rt, const std::string& st)
{ TRY_SUP_ORACLE(List(kw, rt, st), List(kw, rt, st)); }

SupplierService::SupplierItem SupplierService::getById(int id)
{ TRY_SUP_ORACLE(GetById(id), GetById(id)); }

SupplierService::SupplierItem SupplierService::create(const SupplierItem& item)
{ TRY_SUP_ORACLE(Create(item), Create(item)); }

SupplierService::SupplierItem SupplierService::update(int id, const SupplierItem& item)
{ TRY_SUP_ORACLE(Update(id, item), Update(id, item)); }

void SupplierService::remove(int id)
{ TRY_SUP_ORACLE_VOID(Remove(id), Remove(id)); }

void SupplierService::updateStatus(int id, const std::string& status)
{ TRY_SUP_ORACLE_VOID(UpdateStatus(id, status), UpdateStatus(id, status)); }

SupplierService makeSupplierService() { return SupplierService{}; }

} // namespace mis::services
