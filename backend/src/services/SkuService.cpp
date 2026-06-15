// =============================================================================
// SkuService — 商品/SKU 管理
// Oracle 优先 + 内存降级
// =============================================================================

#include "services/SkuService.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <algorithm>
#include <iostream>
#include <mutex>
#include <ctime>

namespace mis::services {

// ---- 辅助 ----
static std::string todayStr()
{
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

static SkuService::SkuItem rowToSku(const std::unordered_map<std::string, std::string>& r)
{
    SkuService::SkuItem s;
    s.id = std::stoi(r.at("PRODUCT_ID"));
    s.skuCode = r.count("SKU_CODE") ? r.at("SKU_CODE") : "";
    s.name = r.at("PRODUCT_NAME");
    s.category = r.count("CATEGORY_NAME") ? r.at("CATEGORY_NAME") : "";
    s.unit = r.count("UNIT") ? r.at("UNIT") : "件";
    s.currentStock = r.count("CURRENT_STOCK") ? std::stoi(r.at("CURRENT_STOCK")) : 0;
    s.safetyStock = r.count("SAFETY_STOCK") ? std::stoi(r.at("SAFETY_STOCK")) : 0;
    s.status = r.count("STATUS") ? r.at("STATUS") : "active";
    s.updatedAt = r.count("UPDATED_AT") ? r.at("UPDATED_AT") : "";
    return s;
}

// ---- Oracle 可用性 ----
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
        std::cout << "[SKU] Oracle 检测通过，使用 Oracle 存储\n";
    } catch (const std::exception& ex) {
        std::cerr << "[SKU] Oracle 不可用: " << ex.what() << "，降级内存存储\n";
        available = false;
    } catch (...) {
        std::cerr << "[SKU] Oracle 不可用: 未知异常，降级内存存储\n";
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

static std::vector<SkuService::SkuItem> oracleList(const std::string& keyword,
                                                     const std::string& category,
                                                     const std::string& status,
                                                     int warehouseId,
                                                     bool lowStockOnly)
{
    mis::dao::DbSessionGuard db;

    std::string sql =
        "SELECT p.product_id, p.sku_code, p.product_name, c.category_name, "
        "p.unit, NVL(i.quantity, 0) AS current_stock, "
        "NVL(i.safety_stock, 0) AS safety_stock, p.status, "
        "TO_CHAR(GREATEST(p.created_at, NVL(i.updated_at, p.created_at)), 'YYYY-MM-DD') AS updated_at "
        "FROM products p "
        "LEFT JOIN categories c ON p.category_id = c.category_id "
        "LEFT JOIN inventory i ON p.product_id = i.product_id "
        "AND i.warehouse_id = :wh WHERE 1=1";

    std::unordered_map<std::string, std::string> binds;
    binds["wh"] = std::to_string(warehouseId);
    if (!keyword.empty()) {
        sql += " AND (UPPER(p.product_name) LIKE UPPER(:kw) OR UPPER(p.sku_code) LIKE UPPER(:kw2))";
        binds["kw"] = "%" + keyword + "%";
        binds["kw2"] = "%" + keyword + "%";
    }
    if (!category.empty()) {
        sql += " AND c.category_name = :cat";
        binds["cat"] = category;
    }
    if (!status.empty()) {
        sql += " AND p.status = :st";
        binds["st"] = status;
    }
    if (lowStockOnly) {
        sql += " AND NVL(i.quantity, 0) < NVL(i.safety_stock, 10)";
    }
    sql += " ORDER BY p.product_id";

    auto rows = mis::dao::oracle().query(sql, binds);
    std::vector<SkuService::SkuItem> result;
    for (const auto& r : rows)
        result.push_back(rowToSku(r));
    return result;
}

static SkuService::SkuItem oracleGetById(int id, int warehouseId)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT p.product_id, p.sku_code, p.product_name, c.category_name, "
        "p.unit, NVL(i.quantity, 0) AS current_stock, "
        "NVL(i.safety_stock, 0) AS safety_stock, p.status, "
        "TO_CHAR(GREATEST(p.created_at, NVL(i.updated_at, p.created_at)), 'YYYY-MM-DD') AS updated_at "
        "FROM products p "
        "LEFT JOIN categories c ON p.category_id = c.category_id "
        "LEFT JOIN inventory i ON p.product_id = i.product_id AND i.warehouse_id = :wh "
        "WHERE p.product_id = :id",
        {{"id", std::to_string(id)}, {"wh", std::to_string(warehouseId)}}
    );
    if (rows.empty()) throw std::runtime_error("SKU not found");
    return rowToSku(rows[0]);
}

static SkuService::SkuItem oracleCreate(const SkuService::SkuItem& item)
{
    mis::dao::DbSessionGuard db;

    // 解析 category_id
    int categoryId = 0;
    if (!item.category.empty()) {
        auto catRows = mis::dao::oracle().query(
            "SELECT category_id FROM categories WHERE category_name = :cn",
            {{"cn", item.category}}
        );
        if (!catRows.empty()) categoryId = std::stoi(catRows[0].at("CATEGORY_ID"));
    }
    if (categoryId == 0) {
        // fallback: 使用第一个分类
        auto catRows = mis::dao::oracle().query(
            "SELECT MIN(category_id) AS ID FROM categories");
        if (!catRows.empty()) categoryId = std::stoi(catRows[0].at("ID"));
    }

    auto seqRow = mis::dao::oracle().query("SELECT seq_products.NEXTVAL AS ID FROM DUAL");
    int newId = std::stoi(seqRow[0].at("ID"));

    mis::dao::oracle().execute(
        "INSERT INTO products (product_id, product_name, sku_code, category_id, unit, unit_price, status) "
        "VALUES (:id, :name, :sku, :cid, :unit, 0, :status)",
        {{"id", std::to_string(newId)}, {"name", item.name}, {"sku", item.skuCode},
         {"cid", std::to_string(categoryId)}, {"unit", item.unit.empty() ? "件" : item.unit},
         {"status", item.status.empty() ? "active" : item.status}}
    );

    if (item.currentStock > 0) {
        auto invSeq = mis::dao::oracle().query("SELECT seq_inventory.NEXTVAL AS ID FROM DUAL");
        mis::dao::oracle().execute(
            "INSERT INTO inventory (inventory_id, product_id, quantity, safety_stock, version, updated_at) "
            "VALUES (:iid, :pid, :qty, :ss, 1, SYSTIMESTAMP)",
            {{"iid", invSeq[0].at("ID")}, {"pid", std::to_string(newId)},
             {"qty", std::to_string(item.currentStock)},
             {"ss", std::to_string(item.safetyStock)}}
        );
    }
    mis::dao::oracle().commit();

    return oracleGetById(newId, 1);
}

static SkuService::SkuItem oracleUpdate(int id, const SkuService::SkuItem& item)
{
    mis::dao::DbSessionGuard db;

    // 确认存在
    auto rows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM products WHERE product_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty() || std::stoi(rows[0].at("CNT")) == 0)
        throw std::runtime_error("SKU not found");

    std::string sql = "UPDATE products SET ";
    std::unordered_map<std::string, std::string> binds;
    bool first = true;
    auto add = [&](const std::string& col, const std::string& val) {
        if (!first) sql += ", ";
        sql += col + " = :" + col;
        binds[col] = val;
        first = false;
    };

    if (!item.skuCode.empty()) add("sku_code", item.skuCode);
    if (!item.name.empty()) add("product_name", item.name);
    if (!item.unit.empty()) add("unit", item.unit);
    if (!item.status.empty()) add("status", item.status);

    if (!item.category.empty()) {
        auto catRows = mis::dao::oracle().query(
            "SELECT category_id FROM categories WHERE category_name = :cn",
            {{"cn", item.category}});
        if (!catRows.empty()) add("category_id", catRows[0].at("CATEGORY_ID"));
    }

    if (!first) {
        sql += " WHERE product_id = :pid";
        binds["pid"] = std::to_string(id);
        mis::dao::oracle().execute(sql, binds);
    }

    // 更新库存
    auto invRows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM inventory WHERE product_id = :pid",
        {{"pid", std::to_string(id)}});
    if (std::stoi(invRows[0].at("CNT")) > 0) {
        mis::dao::oracle().execute(
            "UPDATE inventory SET quantity = :qty, safety_stock = :ss, "
            "version = version + 1, updated_at = SYSTIMESTAMP "
            "WHERE product_id = :pid",
            {{"qty", std::to_string(item.currentStock)},
             {"ss", std::to_string(item.safetyStock)},
             {"pid", std::to_string(id)}});
    } else if (item.currentStock > 0 || item.safetyStock > 0) {
        auto invSeq = mis::dao::oracle().query("SELECT seq_inventory.NEXTVAL AS ID FROM DUAL");
        mis::dao::oracle().execute(
            "INSERT INTO inventory (inventory_id, product_id, quantity, safety_stock, version, updated_at) "
            "VALUES (:iid, :pid, :qty, :ss, 1, SYSTIMESTAMP)",
            {{"iid", invSeq[0].at("ID")}, {"pid", std::to_string(id)},
             {"qty", std::to_string(item.currentStock)},
             {"ss", std::to_string(item.safetyStock)}});
    }
    mis::dao::oracle().commit();

    return oracleGetById(id, 1);
}

static void oracleRemove(int id)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "DELETE FROM inventory WHERE product_id = :pid",
        {{"pid", std::to_string(id)}});
    mis::dao::oracle().execute(
        "DELETE FROM products WHERE product_id = :pid",
        {{"pid", std::to_string(id)}});
    mis::dao::oracle().commit();
}

static void oracleUpdateStatus(int id, const std::string& status)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM products WHERE product_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty() || std::stoi(rows[0].at("CNT")) == 0)
        throw std::runtime_error("SKU not found");
    mis::dao::oracle().execute(
        "UPDATE products SET status = :st WHERE product_id = :id",
        {{"st", status}, {"id", std::to_string(id)}});
    mis::dao::oracle().commit();
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储（降级模式）
// =========================================================================

namespace {

struct MemSku {
    int id;
    std::string skuCode;
    std::string name;
    std::string category;
    std::string unit;
    std::string supplierName;
    int currentStock;
    int safetyStock;
    std::string status;
    std::string updatedAt;
};

std::vector<MemSku> memSkus;
std::mutex memMutex;
int memNextId = 2001;

void initMemSkus()
{
    if (!memSkus.empty()) return;
    memSkus.push_back({1001, "SKU-RF-001", "手持扫码终端", "设备", "台", "华东智造供应链", 128, 30, "active", "2026-05-20"});
    memSkus.push_back({1002, "SKU-PK-018", "标准周转箱",   "耗材", "箱", "青禾包装",         22, 40, "active", "2026-05-21"});
    memSkus.push_back({1003, "SKU-LB-206", "防水标签纸",   "耗材", "卷", "北辰纸业",        480,120, "active", "2026-05-22"});
    memSkus.push_back({1004, "SKU-PT-066", "轻型托盘",     "仓储", "个", "启明仓储设备",      0, 20, "disabled","2026-05-16"});
    memNextId = 2001;
}

SkuService::SkuItem memToSku(const MemSku& m)
{
    SkuService::SkuItem s;
    s.id = m.id; s.skuCode = m.skuCode; s.name = m.name;
    s.category = m.category; s.unit = m.unit;
    s.supplierName = m.supplierName;
    s.currentStock = m.currentStock; s.safetyStock = m.safetyStock;
    s.status = m.status; s.updatedAt = m.updatedAt;
    return s;
}

} // namespace

static std::vector<SkuService::SkuItem> memList(const std::string& keyword,
                                                  const std::string& category,
                                                  const std::string& status,
                                                  int /*warehouseId*/,
                                                  bool lowStockOnly)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    std::vector<SkuService::SkuItem> result;
    for (const auto& s : memSkus) {
        if (!keyword.empty()) {
            std::string kw = keyword;
            std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
            std::string code = s.skuCode, name = s.name;
            std::transform(code.begin(), code.end(), code.begin(), ::tolower);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (code.find(kw) == std::string::npos && name.find(kw) == std::string::npos)
                continue;
        }
        if (!category.empty() && s.category != category) continue;
        if (!status.empty() && s.status != status) continue;
        if (lowStockOnly && s.currentStock >= s.safetyStock) continue;
        result.push_back(memToSku(s));
    }
    return result;
}

static SkuService::SkuItem memGetById(int id, int /*warehouseId*/)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    for (const auto& s : memSkus)
        if (s.id == id) return memToSku(s);
    throw std::runtime_error("SKU not found");
}

static SkuService::SkuItem memCreate(const SkuService::SkuItem& item)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    MemSku m;
    m.id = memNextId++;
    m.skuCode = item.skuCode;
    m.name = item.name;
    m.category = item.category.empty() ? "耗材" : item.category;
    m.unit = item.unit.empty() ? "件" : item.unit;
    m.currentStock = item.currentStock;
    m.safetyStock = item.safetyStock;
    m.status = item.status.empty() ? "active" : item.status;
    m.updatedAt = todayStr();
    memSkus.insert(memSkus.begin(), m);
    return memToSku(m);
}

static SkuService::SkuItem memUpdate(int id, const SkuService::SkuItem& item)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    for (auto& s : memSkus) {
        if (s.id != id) continue;
        if (!item.skuCode.empty()) s.skuCode = item.skuCode;
        if (!item.name.empty()) s.name = item.name;
        if (!item.category.empty()) s.category = item.category;
        if (!item.unit.empty()) s.unit = item.unit;
        if (item.currentStock >= 0) s.currentStock = item.currentStock;
        if (item.safetyStock >= 0) s.safetyStock = item.safetyStock;
        if (!item.status.empty()) s.status = item.status;
        s.updatedAt = todayStr();
        return memToSku(s);
    }
    throw std::runtime_error("SKU not found");
}

static void memRemove(int id)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    auto it = std::remove_if(memSkus.begin(), memSkus.end(),
        [id](const MemSku& s) { return s.id == id; });
    if (it != memSkus.end()) memSkus.erase(it, memSkus.end());
}

static void memUpdateStatus(int id, const std::string& status)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemSkus();
    for (auto& s : memSkus) {
        if (s.id == id) { s.status = status; return; }
    }
    throw std::runtime_error("SKU not found");
}

// =========================================================================
// 统一接口
// =========================================================================

#ifdef MIS_HAS_ORACLE
#define TRY_SKU_ORACLE(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { return oracle##call; } catch (const std::exception& ex) { \
                std::cerr << "[SKU] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        return mem##call; \
    } while(0)

#define TRY_SKU_ORACLE_VOID(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { oracle##call; return; } catch (const std::exception& ex) { \
                std::cerr << "[SKU] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        mem##call; \
    } while(0)
#else
#define TRY_SKU_ORACLE(call, fallback) return mem##call;
#define TRY_SKU_ORACLE_VOID(call, fallback) mem##call;
#endif

std::vector<SkuService::SkuItem> SkuService::list(
    const std::string& keyword, const std::string& category, const std::string& status,
    int warehouseId, bool lowStockOnly)
{
    TRY_SKU_ORACLE(List(keyword, category, status, warehouseId, lowStockOnly),
                   List(keyword, category, status, warehouseId, lowStockOnly));
}

SkuService::SkuItem SkuService::getById(int id, int warehouseId)
{
    TRY_SKU_ORACLE(GetById(id, warehouseId), GetById(id, warehouseId));
}

SkuService::SkuItem SkuService::create(const SkuItem& item)
{
    TRY_SKU_ORACLE(Create(item), Create(item));
}

SkuService::SkuItem SkuService::update(int id, const SkuItem& item)
{
    TRY_SKU_ORACLE(Update(id, item), Update(id, item));
}

void SkuService::remove(int id)
{
    TRY_SKU_ORACLE_VOID(Remove(id), Remove(id));
}

void SkuService::updateStatus(int id, const std::string& status)
{
    TRY_SKU_ORACLE_VOID(UpdateStatus(id, status), UpdateStatus(id, status));
}

SkuService makeSkuService() { return SkuService{}; }

} // namespace mis::services
