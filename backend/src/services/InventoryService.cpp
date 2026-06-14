// =============================================================================
// InventoryService — 入库业务服务
//
// Oracle 优先：直接 SQL 操作（proc_inbound_* 存储过程保留在 DB 供参考）
// 内存降级：Oracle 不可用时自动切换到内存存储
// =============================================================================

#include "models/InventoryModel.hpp"
#include "services/InventoryService.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

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
    try {
        mis::dao::oracle().acquireForCurrentThread();
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        mis::dao::oracle().releaseForCurrentThread();
        return true;
    } catch (...) {
        return false;
    }
#else
    return false;
#endif
}

// =========================================================================
// Oracle 模式 — 直接 SQL
// =========================================================================
#ifdef MIS_HAS_ORACLE

static models::InboundOrder oracleCreateInbound(const models::InboundRequest& req)
{
    mis::dao::DbSessionGuard db;

    // 获取序列值
    auto seqRow = mis::dao::oracle().query("SELECT seq_inbound_orders.NEXTVAL AS ID FROM DUAL");
    int inboundId = std::stoi(seqRow[0].at("ID"));

    // 插入主记录
    mis::dao::oracle().execute(
        "INSERT INTO inbound_orders (inbound_id, supplier_id, status, created_by, created_at, remark) "
        "VALUES (:id, :sid, 'DRAFT', :cb, SYSTIMESTAMP, :rmk)",
        {{"id", std::to_string(inboundId)},
         {"sid", std::to_string(req.supplierId)},
         {"cb", std::to_string(req.createdBy)},
         {"rmk", req.remark}}
    );

    models::InboundOrder result;
    result.inboundId = inboundId;
    result.supplierId = req.supplierId;
    result.status = "DRAFT";
    result.createdBy = req.createdBy;
    result.createdAt = nowISO();
    result.remark = req.remark;

    for (const auto& l : req.lines) {
        auto lineSeq = mis::dao::oracle().query("SELECT seq_inbound_order_lines.NEXTVAL AS ID FROM DUAL");
        int lineId = std::stoi(lineSeq[0].at("ID"));

        mis::dao::oracle().execute(
            "INSERT INTO inbound_order_lines (line_id, inbound_id, product_id, quantity_ordered, quantity_received, unit_price) "
            "VALUES (:lid, :iid, :pid, :qty, 0, :price)",
            {{"lid", std::to_string(lineId)},
             {"iid", std::to_string(inboundId)},
             {"pid", std::to_string(l.productId)},
             {"qty", std::to_string(l.quantityOrdered)},
             {"price", std::to_string(l.unitPrice)}}
        );

        models::InboundLine rl = l;
        rl.lineId = lineId;
        rl.quantityReceived = 0;
        result.lines.push_back(rl);
    }

    mis::dao::oracle().commit();
    return result;
}

static void oracleSubmitInbound(int inboundId)
{
    mis::dao::DbSessionGuard db;
    mis::dao::oracle().execute(
        "UPDATE inbound_orders SET status = 'SUBMITTED' WHERE inbound_id = :id AND status = 'DRAFT'",
        {{"id", std::to_string(inboundId)}}
    );
    mis::dao::oracle().commit();
}

static void oracleCancelInbound(int inboundId)
{
    mis::dao::DbSessionGuard db;

    // 回退已收库存
    auto lines = mis::dao::oracle().query(
        "SELECT product_id, quantity_received FROM inbound_order_lines "
        "WHERE inbound_id = :id AND quantity_received > 0",
        {{"id", std::to_string(inboundId)}}
    );
    for (const auto& l : lines) {
        int pid = std::stoi(l.at("PRODUCT_ID"));
        int qty = std::stoi(l.at("QUANTITY_RECEIVED"));
        mis::dao::oracle().execute(
            "UPDATE inventory SET quantity = quantity - :qty, version = version + 1, updated_at = SYSTIMESTAMP "
            "WHERE product_id = :pid",
            {{"qty", std::to_string(qty)}, {"pid", std::to_string(pid)}}
        );
        mis::dao::oracle().execute(
            "UPDATE inbound_order_lines SET quantity_received = 0 WHERE inbound_id = :id AND product_id = :pid",
            {{"id", std::to_string(inboundId)}, {"pid", std::to_string(pid)}}
        );
    }

    mis::dao::oracle().execute(
        "UPDATE inbound_orders SET status = 'CANCELLED' WHERE inbound_id = :id AND status != 'RECEIVED'",
        {{"id", std::to_string(inboundId)}}
    );
    mis::dao::oracle().commit();
}

static void oracleReceiveLine(int lineId, double quantity)
{
    mis::dao::DbSessionGuard db;

    // 查明细行
    auto lineRows = mis::dao::oracle().query(
        "SELECT iol.product_id, iol.inbound_id, iol.quantity_ordered, iol.quantity_received, io.status "
        "FROM inbound_order_lines iol JOIN inbound_orders io ON iol.inbound_id = io.inbound_id "
        "WHERE iol.line_id = :lid",
        {{"lid", std::to_string(lineId)}}
    );
    if (lineRows.empty()) throw models::ValidationError("明细行不存在");

    const auto& lr = lineRows[0];
    std::string status = lr.at("STATUS");
    int productId = std::stoi(lr.at("PRODUCT_ID"));
    int ordered = std::stoi(lr.at("QUANTITY_ORDERED"));
    int received = std::stoi(lr.at("QUANTITY_RECEIVED"));

    if (status == "CANCELLED") throw models::ValidationError("入库单已取消");
    if (status == "RECEIVED") throw models::ValidationError("入库单已全部到货");
    if (quantity <= 0 || received + quantity > ordered)
        throw models::ValidationError("收货量无效");

    // 更新明细行
    int newReceived = received + static_cast<int>(quantity);
    mis::dao::oracle().execute(
        "UPDATE inbound_order_lines SET quantity_received = :qty WHERE line_id = :lid",
        {{"qty", std::to_string(newReceived)}, {"lid", std::to_string(lineId)}}
    );

    // 更新库存（首次插入或累加）
    auto invRows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM inventory WHERE product_id = :pid",
        {{"pid", std::to_string(productId)}}
    );
    int cnt = std::stoi(invRows[0].at("CNT"));
    if (cnt == 0) {
        auto seqRow = mis::dao::oracle().query("SELECT seq_inventory.NEXTVAL AS ID FROM DUAL");
        int invId = std::stoi(seqRow[0].at("ID"));
        mis::dao::oracle().execute(
            "INSERT INTO inventory (inventory_id, product_id, quantity, version, updated_at) "
            "VALUES (:iid, :pid, :qty, 1, SYSTIMESTAMP)",
            {{"iid", std::to_string(invId)}, {"pid", std::to_string(productId)}, {"qty", std::to_string(static_cast<int>(quantity))}}
        );
    } else {
        mis::dao::oracle().execute(
            "UPDATE inventory SET quantity = quantity + :qty, version = version + 1, updated_at = SYSTIMESTAMP "
            "WHERE product_id = :pid",
            {{"qty", std::to_string(static_cast<int>(quantity))}, {"pid", std::to_string(productId)}}
        );
    }

    // 更新入库单状态
    auto summary = mis::dao::oracle().query(
        "SELECT COUNT(*) AS TOTAL, "
        "SUM(CASE WHEN quantity_received >= quantity_ordered THEN 1 ELSE 0 END) AS FULL_CNT "
        "FROM inbound_order_lines WHERE inbound_id = (SELECT inbound_id FROM inbound_order_lines WHERE line_id = :lid)",
        {{"lid", std::to_string(lineId)}}
    );
    int total = std::stoi(summary[0].at("TOTAL"));
    int fullCnt = std::stoi(summary[0].at("FULL_CNT"));
    std::string newStatus = (fullCnt == total) ? "RECEIVED" : "PARTIAL";

    mis::dao::oracle().execute(
        std::string("UPDATE inbound_orders SET status = '") + newStatus +
        (newStatus == "RECEIVED" ? "', received_at = SYSTIMESTAMP" : "'") +
        " WHERE inbound_id = (SELECT inbound_id FROM inbound_order_lines WHERE line_id = :lid)",
        {{"lid", std::to_string(lineId)}}
    );

    mis::dao::oracle().commit();
}

static models::InboundOrder oracleGetInbound(int inboundId)
{
    mis::dao::DbSessionGuard db;

    auto rows = mis::dao::oracle().query(
        "SELECT inbound_id, supplier_id, status, created_by, "
        "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS') AS created_at, "
        "TO_CHAR(received_at, 'YYYY-MM-DD\"T\"HH24:MI:SS') AS received_at, "
        "remark FROM inbound_orders WHERE inbound_id = :id",
        {{"id", std::to_string(inboundId)}}
    );

    models::InboundOrder order;
    if (rows.empty()) return order;

    const auto& r = rows[0];
    order.inboundId = std::stoi(r.at("INBOUND_ID"));
    order.supplierId = std::stoi(r.at("SUPPLIER_ID"));
    order.status = r.at("STATUS");
    order.createdBy = std::stoi(r.at("CREATED_BY"));
    order.createdAt = r.at("CREATED_AT");
    order.receivedAt = r.count("RECEIVED_AT") && r.at("RECEIVED_AT") != "" ? r.at("RECEIVED_AT") : "";
    order.remark = r.count("REMARK") ? r.at("REMARK") : "";

    auto lineRows = mis::dao::oracle().query(
        "SELECT line_id, product_id, quantity_ordered, quantity_received, unit_price "
        "FROM inbound_order_lines WHERE inbound_id = :id ORDER BY line_id",
        {{"id", std::to_string(inboundId)}}
    );
    for (const auto& lr : lineRows) {
        models::InboundLine line;
        line.lineId = std::stoi(lr.at("LINE_ID"));
        line.productId = std::stoi(lr.at("PRODUCT_ID"));
        line.quantityOrdered = std::stoi(lr.at("QUANTITY_ORDERED"));
        line.quantityReceived = std::stoi(lr.at("QUANTITY_RECEIVED"));
        line.unitPrice = std::stod(lr.at("UNIT_PRICE"));
        order.lines.push_back(line);
    }

    return order;
}

static std::vector<models::InboundOrder> oracleListInbound(
    const std::string& status, int supplierId, int limit, int offset)
{
    mis::dao::DbSessionGuard db;

    std::ostringstream sql;
    sql << "SELECT inbound_id, supplier_id, status, created_by, "
           "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS') AS created_at, "
           "TO_CHAR(received_at, 'YYYY-MM-DD\"T\"HH24:MI:SS') AS received_at, "
           "remark FROM inbound_orders WHERE 1=1";

    std::unordered_map<std::string, std::string> binds;
    if (!status.empty()) {
        sql << " AND status = :status";
        binds["status"] = status;
    }
    if (supplierId > 0) {
        sql << " AND supplier_id = :sid";
        binds["sid"] = std::to_string(supplierId);
    }
    sql << " ORDER BY inbound_id DESC OFFSET :offset ROWS FETCH NEXT :limit ROWS ONLY";
    binds["offset"] = std::to_string(offset);
    binds["limit"] = std::to_string(limit);

    auto rows = mis::dao::oracle().query(sql.str(), binds);
    std::vector<models::InboundOrder> orders;
    for (const auto& r : rows) {
        models::InboundOrder o;
        o.inboundId = std::stoi(r.at("INBOUND_ID"));
        o.supplierId = std::stoi(r.at("SUPPLIER_ID"));
        o.status = r.at("STATUS");
        o.createdBy = std::stoi(r.at("CREATED_BY"));
        o.createdAt = r.at("CREATED_AT");
        o.receivedAt = r.count("RECEIVED_AT") && r.at("RECEIVED_AT") != "" ? r.at("RECEIVED_AT") : "";
        o.remark = r.count("REMARK") ? r.at("REMARK") : "";

        // 每个订单查明细
        auto lineRows = mis::dao::oracle().query(
            "SELECT line_id, product_id, quantity_ordered, quantity_received, unit_price "
            "FROM inbound_order_lines WHERE inbound_id = :iid ORDER BY line_id",
            {{"iid", std::to_string(o.inboundId)}}
        );
        for (const auto& lr : lineRows) {
            models::InboundLine line;
            line.lineId = std::stoi(lr.at("LINE_ID"));
            line.productId = std::stoi(lr.at("PRODUCT_ID"));
            line.quantityOrdered = std::stoi(lr.at("QUANTITY_ORDERED"));
            line.quantityReceived = std::stoi(lr.at("QUANTITY_RECEIVED"));
            line.unitPrice = std::stod(lr.at("UNIT_PRICE"));
            o.lines.push_back(line);
        }
        orders.push_back(o);
    }
    return orders;
}

static InventoryService::DashboardStats oracleGetDashboard()
{
    mis::dao::DbSessionGuard db;
    InventoryService::DashboardStats s;

    auto r1 = mis::dao::oracle().query("SELECT NVL(SUM(quantity), 0) AS T FROM inventory");
    if (!r1.empty()) s.totalStock = std::stoi(r1[0].at("T"));

    auto r2 = mis::dao::oracle().query(
        "SELECT NVL(SUM(quantity_received), 0) AS T FROM inbound_order_lines WHERE TRUNC(created_at) = TRUNC(SYSDATE)");
    if (!r2.empty()) s.inboundToday = std::stoi(r2[0].at("T"));

    auto r3 = mis::dao::oracle().query("SELECT COUNT(*) AS T FROM inventory WHERE quantity < 10");
    if (!r3.empty()) s.lowStockSku = std::stoi(r3[0].at("T"));

    auto r4 = mis::dao::oracle().query("SELECT COUNT(*) AS T FROM inbound_orders WHERE status IN ('DRAFT','SUBMITTED','PARTIAL')");
    if (!r4.empty()) s.pendingInbound = std::stoi(r4[0].at("T"));

    return s;
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

static InventoryService::DashboardStats memGetDashboard()
{
    InventoryService::DashboardStats s;
    s.totalStock = 12860;
    s.pendingInbound = 0;
    for (const auto& o : memOrders) {
        if (o.status == "DRAFT" || o.status == "SUBMITTED" || o.status == "PARTIAL") s.pendingInbound++;
        for (const auto& l : o.lines) s.inboundToday += l.quantityReceived;
    }
    s.lowStockSku = 3;
    return s;
}

// =========================================================================
// 统一接口（自动选择 Oracle / 内存）
// =========================================================================

#define TRY_ORACLE(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { return oracle##call; } catch (const std::exception& ex) { \
                std::cerr << "[WMS] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        return mem##call; \
    } while(0)

models::InboundOrder InventoryService::createInbound(const models::InboundRequest& req) {
    validateInbound(req);
    TRY_ORACLE(CreateInbound(req), CreateInbound(req));
}

void InventoryService::submitInbound(int id, int) {
    TRY_ORACLE(SubmitInbound(id), SubmitInbound(id));
}

void InventoryService::cancelInbound(int id, int) {
    TRY_ORACLE(CancelInbound(id), CancelInbound(id));
}

void InventoryService::receiveLine(int lineId, double qty, int) {
    TRY_ORACLE(ReceiveLine(lineId, qty), ReceiveLine(lineId, qty));
}

models::InboundOrder InventoryService::getInbound(int id) {
    TRY_ORACLE(GetInbound(id), GetInbound(id));
}

std::vector<models::InboundOrder> InventoryService::listInbound(
    const std::string& status, int supplierId, int limit, int offset) {
    TRY_ORACLE(ListInbound(status, supplierId, limit, offset), ListInbound(status, supplierId, limit, offset));
}

InventoryService::DashboardStats InventoryService::getDashboard() {
    TRY_ORACLE(GetDashboard(), GetDashboard());
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
