// =============================================================================
// InventoryService — 入库业务服务
//
// Oracle 优先：直接 SQL 操作（proc_inbound_* 存储过程保留在 DB 供参考）
// 内存降级：Oracle 不可用时自动切换到内存存储
// =============================================================================

#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"
#include "utils/RequestContext.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#include "dao/InventoryDAO.hpp"
#endif

#include <mutex>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

namespace mis::services {

// ---- 辅助 ----
static std::string nowISO()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
    return buf;
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
        std::cout << "[WMS] Oracle 检测通过，使用 Oracle 存储\n";
    } catch (const std::exception& ex) {
        std::cerr << "[WMS] Oracle 不可用: " << ex.what() << "，降级内存存储\n";
        available = false;
    } catch (...) {
        std::cerr << "[WMS] Oracle 不可用: 未知异常，降级内存存储\n";
        available = false;
    }
    return available;
#else
    return false;
#endif
}

// =========================================================================
// Oracle 模式 — 直接 SQL
// =========================================================================
#ifdef MIS_HAS_ORACLE

static std::string getEnv(const char* name, const char* fallback)
{
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : value;
}

static wms::dao::InventoryDAO& getInventoryDao()
{
    static std::unique_ptr<wms::dao::InventoryDAO> dao;
    static std::once_flag flag;
    std::call_once(flag, []() {
        const auto dbUrl = getEnv("MIS_DB_URL", "localhost:1522/FREEPDB1");
        const auto dbUser = getEnv("MIS_DB_USER", "wms");
        const auto dbPassword = getEnv("MIS_DB_PASSWORD", "123123");
        dao = std::make_unique<wms::dao::InventoryDAO>(dbUrl, dbUser, dbPassword);
    });
    return *dao;
}

static models::InboundOrder mapOrder(const wms::dao::InboundOrder& src)
{
    models::InboundOrder dest;
    dest.inboundId = static_cast<int>(src.inbound_id);
    dest.supplierId = static_cast<int>(src.supplier_id);
    dest.status = src.status;
    dest.createdBy = static_cast<int>(src.created_by);
    dest.createdAt = src.created_at;
    dest.receivedAt = src.received_at;
    dest.remark = src.remark;
    for (const auto& l : src.lines) {
        models::InboundLine destLine;
        destLine.lineId = static_cast<int>(l.line_id);
        destLine.productId = static_cast<int>(l.product_id);
        destLine.quantityOrdered = static_cast<int>(l.quantity_ordered);
        destLine.quantityReceived = static_cast<int>(l.quantity_received);
        destLine.unitPrice = l.unit_price;
        dest.lines.push_back(destLine);
    }
    return dest;
}

static models::InboundOrder oracleCreateInbound(const models::InboundRequest& req)
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& l : req.lines) {
        arr.push_back({
            {"product_id", l.productId},
            {"quantity", l.quantityOrdered},
            {"unit_price", l.unitPrice}
        });
    }
    std::string linesJson = arr.dump();
    auto res = getInventoryDao().createInboundOrder(req.supplierId, req.createdBy, linesJson);
    if (!res.ok()) {
        throw models::ValidationError(res.message);
    }
    auto dbOrderOpt = getInventoryDao().getInboundOrder(res.inbound_id);
    if (!dbOrderOpt) {
        throw models::ValidationError("无法获取创建的入库单明细");
    }
    return mapOrder(*dbOrderOpt);
}

static void oracleSubmitInbound(int inboundId, int operatorId)
{
    auto res = getInventoryDao().submitInboundOrder(inboundId, operatorId);
    if (!res.ok()) {
        throw models::ValidationError(res.message);
    }
}

static void oracleCancelInbound(int inboundId, int operatorId)
{
    auto res = getInventoryDao().cancelInboundOrder(inboundId, operatorId);
    if (!res.ok()) {
        throw models::ValidationError(res.message);
    }
}

static void oracleReceiveLine(int lineId, double quantity, int operatorId)
{
    auto res = getInventoryDao().receiveInbound(lineId, quantity, operatorId, std::nullopt);
    if (!res.ok()) {
        throw models::ValidationError(res.message);
    }
}

static models::InboundOrder oracleGetInbound(int inboundId)
{
    auto dbOrderOpt = getInventoryDao().getInboundOrder(inboundId);
    if (!dbOrderOpt) return {};
    return mapOrder(*dbOrderOpt);
}

static std::vector<models::InboundOrder> oracleListInbound(
    const std::string& status, int supplierId, int limit, int offset)
{
    auto list = getInventoryDao().listInboundOrders(status, supplierId, limit, offset);
    std::vector<models::InboundOrder> result;
    for (const auto& o : list) {
        result.push_back(mapOrder(o));
    }
    return result;
}

static InventoryService::DashboardStats oracleGetDashboard(int warehouseId)
{
    mis::dao::DbSessionGuard db;
    InventoryService::DashboardStats s;

    auto r1 = mis::dao::oracle().query(
        "SELECT NVL(SUM(quantity), 0) AS T FROM inventory WHERE warehouse_id = :wh",
        {{"wh", std::to_string(warehouseId)}});
    if (!r1.empty()) s.totalStock = std::stoi(r1[0].at("T"));

    auto r2 = mis::dao::oracle().query(
        "SELECT NVL(SUM(iol.quantity_received), 0) AS T "
        "FROM inbound_order_lines iol "
        "JOIN inbound_orders io ON iol.inbound_id = io.inbound_id "
        "WHERE TRUNC(io.created_at) = TRUNC(SYSDATE)");
    if (!r2.empty()) s.inboundToday = std::stoi(r2[0].at("T"));

    auto r3 = mis::dao::oracle().query(
        "SELECT COUNT(*) AS T FROM inventory "
        "WHERE warehouse_id = :wh AND quantity < NVL(safety_stock, 10)",
        {{"wh", std::to_string(warehouseId)}});
    if (!r3.empty()) s.lowStockSku = std::stoi(r3[0].at("T"));

    auto r4 = mis::dao::oracle().query("SELECT COUNT(*) AS T FROM inbound_orders WHERE status IN ('DRAFT','SUBMITTED','PARTIAL')");
    if (!r4.empty()) s.pendingInbound = std::stoi(r4[0].at("T"));

    auto r5 = mis::dao::oracle().query("SELECT COUNT(*) AS T FROM inbound_orders WHERE status = 'CANCELLED'");
    if (!r5.empty()) s.exceptionCount = std::stoi(r5[0].at("T"));

    return s;
}

static InventoryService::ReceiveBySkuResult oracleReceiveBySku(
    const std::string& skuCode, int productId, int quantity)
{
    mis::dao::DbSessionGuard db;
    InventoryService::ReceiveBySkuResult result;

    // 1. 查找待收货的入库单明细行（FIFO：按入库单创建时间排序）
    auto lines = mis::dao::oracle().query(
        "SELECT iol.line_id, iol.inbound_id, iol.quantity_ordered, iol.quantity_received, io.status "
        "FROM inbound_order_lines iol "
        "JOIN inbound_orders io ON iol.inbound_id = io.inbound_id "
        "WHERE iol.product_id = :pid "
        "  AND io.status IN ('SUBMITTED', 'PARTIAL') "
        "  AND iol.quantity_received < iol.quantity_ordered "
        "ORDER BY io.created_at ASC",
        {{"pid", std::to_string(productId)}}
    );

    if (lines.empty()) {
        result.success = false;
        result.message = "没有找到该商品对应的待收货入库单（需要先创建并提交采购入库单）";
        return result;
    }

    // 2. FIFO 收货
    int remaining = quantity;
    for (const auto& lr : lines) {
        if (remaining <= 0) break;

        int lineId = std::stoi(lr.at("LINE_ID"));
        int ordered = std::stoi(lr.at("QUANTITY_ORDERED"));
        int received = std::stoi(lr.at("QUANTITY_RECEIVED"));
        int inboundId = std::stoi(lr.at("INBOUND_ID"));
        int canReceive = ordered - received;
        int toReceive = std::min(remaining, canReceive);
        int newReceived = received + toReceive;

        // 更新明细行
        mis::dao::oracle().execute(
            "UPDATE inbound_order_lines SET quantity_received = :qty WHERE line_id = :lid",
            {{"qty", std::to_string(newReceived)}, {"lid", std::to_string(lineId)}}
        );

        // 更新库存
        auto invRows = mis::dao::oracle().query(
            "SELECT COUNT(*) AS CNT FROM inventory WHERE product_id = :pid",
            {{"pid", std::to_string(productId)}}
        );
        if (std::stoi(invRows[0].at("CNT")) == 0) {
            auto seqRow = mis::dao::oracle().query("SELECT seq_inventory.NEXTVAL AS ID FROM DUAL");
            mis::dao::oracle().execute(
                "INSERT INTO inventory (inventory_id, product_id, quantity, version, updated_at) "
                "VALUES (:iid, :pid, :qty, 1, SYSTIMESTAMP)",
                {{"iid", seqRow[0].at("ID")}, {"pid", std::to_string(productId)}, {"qty", std::to_string(toReceive)}}
            );
        } else {
            mis::dao::oracle().execute(
                "UPDATE inventory SET quantity = quantity + :qty, version = version + 1, updated_at = SYSTIMESTAMP "
                "WHERE product_id = :pid",
                {{"qty", std::to_string(toReceive)}, {"pid", std::to_string(productId)}}
            );
        }

        // 更新入库单状态
        auto summary = mis::dao::oracle().query(
            "SELECT COUNT(*) AS TOTAL, "
            "SUM(CASE WHEN quantity_received >= quantity_ordered THEN 1 ELSE 0 END) AS FULL_CNT "
            "FROM inbound_order_lines WHERE inbound_id = :iid",
            {{"iid", std::to_string(inboundId)}}
        );
        int total = std::stoi(summary[0].at("TOTAL"));
        int fullCnt = std::stoi(summary[0].at("FULL_CNT"));
        std::string newStatus = (fullCnt == total) ? "RECEIVED" : "PARTIAL";

        mis::dao::oracle().execute(
            std::string("UPDATE inbound_orders SET status = '") + newStatus +
            (newStatus == "RECEIVED" ? "', received_at = SYSTIMESTAMP" : "'") +
            " WHERE inbound_id = :iid",
            {{"iid", std::to_string(inboundId)}}
        );

        result.orderIds.push_back(inboundId);
        result.totalReceived += toReceive;
        remaining -= toReceive;
    }

    mis::dao::oracle().commit();

    result.success = true;
    if (remaining > 0) {
        result.message = "部分收货成功：已收 " + std::to_string(result.totalReceived)
            + "，剩余 " + std::to_string(remaining) + " 无可收明细";
    } else {
        result.message = "收货成功：共 " + std::to_string(result.totalReceived) + " 件";
    }
    return result;
}

static std::vector<InventoryService::TrendPoint> oracleGetRecentTrend(int days)
{
    mis::dao::DbSessionGuard db;
    std::vector<InventoryService::TrendPoint> result;

    auto rows = mis::dao::oracle().query(
        "SELECT TO_CHAR(TRUNC(io.created_at), 'MM-DD') AS dt, "
        "NVL(SUM(iol.quantity_received), 0) AS qty "
        "FROM inbound_orders io "
        "JOIN inbound_order_lines iol ON io.inbound_id = iol.inbound_id "
        "WHERE io.created_at >= TRUNC(SYSDATE) - :days "
        "GROUP BY TRUNC(io.created_at) ORDER BY dt",
        {{"days", std::to_string(days)}}
    );

    for (const auto& r : rows) {
        result.push_back({r.at("DT"), std::stoi(r.at("QTY"))});
    }
    return result;
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储（降级模式）
// =========================================================================

static std::vector<models::InboundOrder> memOrders;
static int memNextInboundId = 1;
static int memNextLineId = 1;

static models::InboundOrder memCreateInbound(const models::InboundRequest& req)
{
    models::InboundOrder o;
    o.inboundId = memNextInboundId++;
    o.supplierId = req.supplierId;
    o.status = "DRAFT";
    o.createdBy = req.createdBy;
    o.createdAt = nowISO();
    o.remark = req.remark;
    for (const auto& l : req.lines) {
        models::InboundLine rl = l;
        rl.lineId = memNextLineId++;
        rl.quantityReceived = 0;
        o.lines.push_back(rl);
    }
    memOrders.insert(memOrders.begin(), o);
    return o;
}

static void memSubmitInbound(int id)
{
    for (auto& o : memOrders) {
        if (o.inboundId == id && o.status == "DRAFT") { o.status = "SUBMITTED"; return; }
    }
    throw models::ValidationError("入库单不存在或非草稿状态");
}

static void memCancelInbound(int id)
{
    for (auto& o : memOrders) {
        if (o.inboundId == id) {
            if (o.status == "CANCELLED") throw models::ValidationError("已取消");
            if (o.status == "RECEIVED") throw models::ValidationError("已到货不可取消");
            o.status = "CANCELLED";
            for (auto& l : o.lines) l.quantityReceived = 0;
            return;
        }
    }
    throw models::ValidationError("入库单不存在");
}

static void memReceiveLine(int lineId, double quantity)
{
    for (auto& order : memOrders) {
        for (auto& line : order.lines) {
            if (line.lineId != lineId) continue;
            if (order.status == "CANCELLED") throw models::ValidationError("已取消");
            if (line.quantityReceived >= line.quantityOrdered) throw models::ValidationError("已到货");
            if (quantity <= 0 || line.quantityReceived + quantity > line.quantityOrdered)
                throw models::ValidationError("收货量无效");

            line.quantityReceived += static_cast<int>(quantity);
            bool all = true;
            for (const auto& l : order.lines)
                if (l.quantityReceived < l.quantityOrdered) { all = false; break; }
            order.status = all ? "RECEIVED" : "PARTIAL";
            if (all) order.receivedAt = nowISO();
            return;
        }
    }
    throw models::ValidationError("明细行不存在");
}

static models::InboundOrder memGetInbound(int id)
{
    for (const auto& o : memOrders) if (o.inboundId == id) return o;
    return {};
}

static std::vector<models::InboundOrder> memListInbound(const std::string& status, int supplierId, int limit, int offset)
{
    std::vector<models::InboundOrder> result;
    for (const auto& o : memOrders) {
        if (!status.empty() && o.status != status) continue;
        if (supplierId > 0 && o.supplierId != supplierId) continue;
        result.push_back(o);
    }
    int s = std::min(offset, static_cast<int>(result.size()));
    int e = std::min(offset + limit, static_cast<int>(result.size()));
    return {result.begin() + s, result.begin() + e};
}

static InventoryService::DashboardStats memGetDashboard(int /*warehouseId*/)
{
    InventoryService::DashboardStats s;
    s.totalStock = 12860;
    s.pendingInbound = 0;
    for (const auto& o : memOrders) {
        if (o.status == "DRAFT" || o.status == "SUBMITTED" || o.status == "PARTIAL") s.pendingInbound++;
        if (o.status == "CANCELLED") s.exceptionCount++;
        for (const auto& l : o.lines) s.inboundToday += l.quantityReceived;
    }
    // 低库存：统计待收货商品中库存不足的（简化逻辑）
    s.lowStockSku = s.totalStock < 100 ? 3 : 0;
    return s;
}

static InventoryService::ReceiveBySkuResult memReceiveBySku(
    const std::string& /*skuCode*/, int productId, int quantity)
{
    InventoryService::ReceiveBySkuResult result;

    // 收集所有待收货的明细行（FIFO：按入库单创建顺序）
    struct PendingLine {
        int lineId;
        int inboundId;
        int ordered;
        int received;
        models::InboundOrder* order;
    };
    std::vector<PendingLine> pending;
    for (auto& order : memOrders) {
        if (order.status != "SUBMITTED" && order.status != "PARTIAL") continue;
        for (auto& line : order.lines) {
            if (line.productId == productId && line.quantityReceived < line.quantityOrdered) {
                pending.push_back({line.lineId, order.inboundId,
                    line.quantityOrdered, line.quantityReceived, &order});
            }
        }
    }

    if (pending.empty()) {
        result.success = false;
        result.message = "没有找到该商品对应的待收货入库单（需要先创建并提交采购入库单）";
        return result;
    }

    // FIFO 收货
    int remaining = quantity;
    for (auto& pl : pending) {
        if (remaining <= 0) break;

        int canReceive = pl.ordered - pl.received;
        int toReceive = std::min(remaining, canReceive);

        // 找到对应 line 并更新
        for (auto& line : pl.order->lines) {
            if (line.lineId == pl.lineId) {
                line.quantityReceived += toReceive;
                break;
            }
        }

        // 更新订单状态
        bool allReceived = true;
        for (const auto& l : pl.order->lines) {
            if (l.quantityReceived < l.quantityOrdered) { allReceived = false; break; }
        }
        pl.order->status = allReceived ? "RECEIVED" : "PARTIAL";
        if (allReceived) pl.order->receivedAt = nowISO();

        result.orderIds.push_back(pl.inboundId);
        result.totalReceived += toReceive;
        remaining -= toReceive;
    }

    result.success = true;
    if (remaining > 0) {
        result.message = "部分收货成功：已收 " + std::to_string(result.totalReceived)
            + "，剩余 " + std::to_string(remaining) + " 无可收明细";
    } else {
        result.message = "收货成功：共 " + std::to_string(result.totalReceived) + " 件";
    }
    return result;
}

static std::vector<InventoryService::TrendPoint> memGetRecentTrend(int days)
{
    std::vector<InventoryService::TrendPoint> result;

    // 生成最近 N 天的日期列表
    std::time_t now = std::time(nullptr);
    for (int i = days - 1; i >= 0; --i) {
        std::time_t day = now - i * 86400;
        char buf[8];
        std::strftime(buf, sizeof(buf), "%m-%d", std::localtime(&day));
        result.push_back({std::string(buf), 0});
    }

    // 统计每天的收货量
    for (const auto& o : memOrders) {
        // 从 createdAt 提取日期
        if (o.createdAt.size() < 10) continue;
        std::string orderDay = o.createdAt.substr(5, 5); // "MM-DD"
        for (auto& tp : result) {
            if (tp.date == orderDay) {
                for (const auto& l : o.lines)
                    tp.quantity += l.quantityReceived;
            }
        }
    }
    return result;
}

// =========================================================================
// 统一接口（自动选择 Oracle / 内存）
// =========================================================================

#ifdef MIS_HAS_ORACLE
#define TRY_ORACLE(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { return oracle##call; } catch (const std::exception& ex) { \
                std::cerr << "[WMS] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        return mem##fallback; \
    } while(0)
#else
#define TRY_ORACLE(call, fallback) return mem##fallback;
#endif

models::InboundOrder InventoryService::createInbound(const models::InboundRequest& req) {
    validateInbound(req);
    TRY_ORACLE(CreateInbound(req), CreateInbound(req));
}

void InventoryService::submitInbound(int id, int operatorId) {
    TRY_ORACLE(SubmitInbound(id, operatorId), SubmitInbound(id));
}

void InventoryService::cancelInbound(int id, int operatorId) {
    TRY_ORACLE(CancelInbound(id, operatorId), CancelInbound(id));
}

void InventoryService::receiveLine(int lineId, double qty, int operatorId) {
    TRY_ORACLE(ReceiveLine(lineId, qty, operatorId), ReceiveLine(lineId, qty));
}

InventoryService::ReceiveBySkuResult InventoryService::receiveBySku(
    const std::string& skuCode, int productId, int quantity) {
    TRY_ORACLE(ReceiveBySku(skuCode, productId, quantity),
               ReceiveBySku(skuCode, productId, quantity));
}

models::InboundOrder InventoryService::getInbound(int id) {
    TRY_ORACLE(GetInbound(id), GetInbound(id));
}

std::vector<models::InboundOrder> InventoryService::listInbound(
    const std::string& status, int supplierId, int limit, int offset) {
    TRY_ORACLE(ListInbound(status, supplierId, limit, offset), ListInbound(status, supplierId, limit, offset));
}

InventoryService::DashboardStats InventoryService::getDashboard(int warehouseId) {
    TRY_ORACLE(GetDashboard(warehouseId), GetDashboard(warehouseId));
}

std::vector<InventoryService::TrendPoint> InventoryService::getRecentTrend(int days, int warehouseId)
{
    TRY_ORACLE(GetRecentTrend(days), GetRecentTrend(days));
}

void InventoryService::validateInbound(const models::InboundRequest& request)
{
    if (request.supplierId <= 0)
        throw models::ValidationError("supplier_id is required");
    if (request.lines.empty())
        throw models::ValidationError("lines must not be empty");
    for (size_t i = 0; i < request.lines.size(); ++i) {
        if (request.lines[i].productId <= 0)
            throw models::ValidationError("line " + std::to_string(i + 1) + ": product_id required");
        if (request.lines[i].quantityOrdered <= 0)
            throw models::ValidationError("line " + std::to_string(i + 1) + ": quantity must be > 0");
    }
}

InventoryService makeInventoryService() { return InventoryService{}; }

} // namespace mis::services
