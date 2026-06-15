// =============================================================================
// WarehouseService — 仓库管理
// Oracle 优先 + 内存降级
// =============================================================================

#include "services/WarehouseService.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <iostream>
#include <mutex>

namespace mis::services {

static WarehouseService::WarehouseItem rowToWh(
    const std::unordered_map<std::string, std::string>& r)
{
    WarehouseService::WarehouseItem w;
    w.id = std::stoi(r.at("WAREHOUSE_ID"));
    w.code = r.at("WAREHOUSE_CODE");
    w.name = r.at("WAREHOUSE_NAME");
    w.address = r.count("ADDRESS") ? r.at("ADDRESS") : "";
    w.status = r.count("STATUS") ? r.at("STATUS") : "active";
    return w;
}

static bool oracleAvailable()
{
#ifdef MIS_HAS_ORACLE
    static bool checked = false, available = false;
    if (checked) return available;
    checked = true;
    try {
        mis::dao::DbSessionGuard db;
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        available = true;
    } catch (...) { available = false; }
    return available;
#else
    return false;
#endif
}

#ifdef MIS_HAS_ORACLE

static std::vector<WarehouseService::WarehouseItem> oracleList()
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT * FROM warehouses WHERE status = 'active' ORDER BY warehouse_id");
    std::vector<WarehouseService::WarehouseItem> result;
    for (const auto& r : rows) result.push_back(rowToWh(r));
    return result;
}

static WarehouseService::WarehouseItem oracleGetById(int id)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT * FROM warehouses WHERE warehouse_id = :id",
        {{"id", std::to_string(id)}});
    if (rows.empty()) throw std::runtime_error("Warehouse not found");
    return rowToWh(rows[0]);
}

static WarehouseService::WarehouseItem oracleGetByCode(const std::string& code)
{
    mis::dao::DbSessionGuard db;
    auto rows = mis::dao::oracle().query(
        "SELECT * FROM warehouses WHERE warehouse_code = :code",
        {{"code", code}});
    if (rows.empty()) throw std::runtime_error("Warehouse not found: " + code);
    return rowToWh(rows[0]);
}

#endif

// 内存降级
namespace {
std::vector<WarehouseService::WarehouseItem> memWh = {
    {1, "DEFAULT", "默认主仓库", "深圳市龙岗区", "active"},
    {2, "SH-HUB", "上海分仓", "上海市松江区", "active"},
    {3, "BJ-HUB", "北京分仓", "北京市顺义区", "active"},
};
std::mutex whMutex;
}

static std::vector<WarehouseService::WarehouseItem> memList()
{
    std::lock_guard<std::mutex> lock(whMutex);
    return memWh;
}

static WarehouseService::WarehouseItem memGetById(int id)
{
    std::lock_guard<std::mutex> lock(whMutex);
    for (const auto& w : memWh) if (w.id == id) return w;
    throw std::runtime_error("Warehouse not found");
}

static WarehouseService::WarehouseItem memGetByCode(const std::string& code)
{
    std::lock_guard<std::mutex> lock(whMutex);
    for (const auto& w : memWh) if (w.code == code) return w;
    throw std::runtime_error("Warehouse not found");
}

#ifdef MIS_HAS_ORACLE
#define TRY_WH_ORACLE(call, fallback) \
    do { if (oracleAvailable()) { try { return oracle##call; } catch (const std::exception& ex) { \
        std::cerr << "[WH] Oracle fail: " << ex.what() << "\n"; } } return mem##call; } while(0)
#else
#define TRY_WH_ORACLE(call, fallback) return mem##call;
#endif

std::vector<WarehouseService::WarehouseItem> WarehouseService::list()
{ TRY_WH_ORACLE(List(), List()); }

WarehouseService::WarehouseItem WarehouseService::getById(int id)
{ TRY_WH_ORACLE(GetById(id), GetById(id)); }

WarehouseService::WarehouseItem WarehouseService::getByCode(const std::string& code)
{ TRY_WH_ORACLE(GetByCode(code), GetByCode(code)); }

WarehouseService makeWarehouseService() { return WarehouseService{}; }

} // namespace mis::services
