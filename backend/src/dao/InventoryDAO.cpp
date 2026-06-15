// =============================================================================
// WMS — InventoryDAO OCCI 实现
//
// 编译要求（MSYS2 ucrt64 + g++，Oracle 23ai Free）：
//   1. Oracle 数据库安装于 D:\tools\26ai\dbhomeFree
//   2. 编译：g++ -std=c++17
//            -I D:/tools/26ai/dbhomeFree/oci/include
//            -L D:/tools/26ai/dbhomeFree/oci/lib/msvc
//            InventoryDAO.cpp -loraocci23 -loci -o ...
//
// 运行要求：
//   - Instant Client 目录在 PATH 或 LD_LIBRARY_PATH
//   - Oracle 数据库实例可访问
// =============================================================================

#include <unordered_map>
#include <string>
#include "InventoryDAO.hpp"

// ---- OCCI headers ----
// occi.h 内部已包含 occiCommon.h / occiData.h / occiControl.h / occiObjects.h / occiAQ.h
#include <occi.h>

#include <sstream>
#include <stdexcept>
#include <cstring>

namespace wms {
namespace dao {

using namespace oracle::occi;

// =========================================================================
// 常量
// =========================================================================
static constexpr int MAX_MSG_LEN       = 4000;
static constexpr int MAX_STATUS_LEN    = 20;
static constexpr int MAX_REMARK_LEN    = 500;
static constexpr int MAX_TIMESTAMP_LEN = 128;

// =========================================================================
// 辅助函数
// =========================================================================
namespace {

/// 安全获取 OUT 字符串参数
std::string safeGetString(Statement* stmt, int param_index) {
    try {
        std::string val = stmt->getString(param_index);
        return val.empty() ? "" : val;
    } catch (const SQLException&) {
        return "";
    }
}

/// 安全获取 OUT 整数参数
int safeGetInt(Statement* stmt, int param_index) {
    try {
        return stmt->getInt(param_index);
    } catch (const SQLException&) {
        return -9999;
    }
}

/// 安全获取 OUT 浮点参数
double safeGetDouble(Statement* stmt, int param_index) {
    try {
        return stmt->getDouble(param_index);
    } catch (const SQLException&) {
        return 0.0;
    }
}

/// 安全获取 OUT 64位整数
/// NOTE: Number 无直接 operator int64_t(long long)，需显式先转 long
int64_t safeGetInt64(Statement* stmt, int param_index) {
    try {
        return static_cast<long>(stmt->getNumber(param_index));
    } catch (const SQLException&) {
        return -9999;
    }
}

/// 安全的 INT64 → Statement set
/// NOTE: Number(int64_t) 存在多条构造重载匹配，显式先转 long 消歧
void setInt64Param(Statement* stmt, int param_index, int64_t val) {
    stmt->setNumber(param_index, Number(static_cast<long>(val)));
}

/// 设置可选参数：若为 nullopt 则传 NULL
void setOptionalInt(Statement* stmt, int param_index, const std::optional<int64_t>& opt) {
    if (opt.has_value()) {
        setInt64Param(stmt, param_index, opt.value());
    } else {
        stmt->setNull(param_index, OCCIINT);
    }
}

/// 执行存储过程调用并获取通用结果（code + message 为最后两个 OUT 参数）
/// @param stmt          已绑定所有参数的 Statement
/// @param code_idx      o_result 参数位序（1-based）
/// @param msg_idx       o_message 参数位序
DaoResult executeAndGetResult(Statement* stmt, int code_idx, int msg_idx) {
    DaoResult r;
    try {
        stmt->executeUpdate();
        r.code    = safeGetInt(stmt, code_idx);
        r.message = safeGetString(stmt, msg_idx);
    } catch (const SQLException& e) {
        r.code    = e.getErrorCode();
        r.message = std::string("Oracle 异常: ") + e.getMessage();
    }
    return r;
}

} // anonymous namespace

// =========================================================================
// PIMPL — 隐藏 OCCI 对象
// =========================================================================
class InventoryDAO::Impl {
public:
    Environment*  env  = nullptr;
    Connection*   conn = nullptr;

    Impl(const std::string& conn_str,
         const std::string& user,
         const std::string& password) {
        env = Environment::createEnvironment(Environment::DEFAULT);
        if (!env) {
            throw std::runtime_error("InventoryDAO: 无法创建 OCCI Environment");
        }
        try {
            conn = env->createConnection(user, password, conn_str);
        } catch (const SQLException& e) {
            Environment::terminateEnvironment(env);
            env = nullptr;
            throw std::runtime_error(
                std::string("InventoryDAO: 连接 Oracle 失败 — ") + e.getMessage());
        }
    }

    ~Impl() {
        if (conn) {
            try { env->terminateConnection(conn); }
            catch (...) {}
            conn = nullptr;
        }
        if (env) {
            try { Environment::terminateEnvironment(env); }
            catch (...) {}
            env = nullptr;
        }
    }

    // 禁止拷贝
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // ---- 辅助：设置审计上下文 ----
    void setOperatorContext(int64_t operator_id) {
        if (operator_id <= 0) return;
        try {
            Statement* stmt = conn->createStatement(
                "BEGIN wms_ctx_pkg.set_operator(:1); END;");
            setInt64Param(stmt, 1, operator_id);
            stmt->executeUpdate();
            conn->terminateStatement(stmt);
        } catch (const SQLException&) {
            // 上下文设置失败不阻塞主流程
        }
    }

    void clearOperatorContext() {
        try {
            Statement* stmt = conn->createStatement(
                "BEGIN wms_ctx_pkg.clear_operator; END;");
            stmt->executeUpdate();
            conn->terminateStatement(stmt);
        } catch (const SQLException&) {
            // ignore
        }
    }
};

// =========================================================================
// InventoryDAO 构造/析构/移动
// =========================================================================
InventoryDAO::InventoryDAO(const std::string& conn_str,
                           const std::string& user,
                           const std::string& password)
    : pImpl_(std::make_unique<Impl>(conn_str, user, password)) {
}

InventoryDAO::~InventoryDAO() = default;

InventoryDAO::InventoryDAO(InventoryDAO&&) noexcept = default;
InventoryDAO& InventoryDAO::operator=(InventoryDAO&&) noexcept = default;

// =========================================================================
// 2.1.1 proc_inbound_create — 创建入库单
// =========================================================================
CreateResult InventoryDAO::createInboundOrder(
    int64_t             supplier_id,
    int64_t             created_by,
    const std::string&  lines_json)
{
    CreateResult r;
    Statement* stmt = nullptr;

    try {
        pImpl_->setOperatorContext(created_by);

        stmt = pImpl_->conn->createStatement(
            "BEGIN proc_inbound_create(:1, :2, :3, :4, :5, :6); END;");

        // IN 参数
        setInt64Param(stmt, 1, supplier_id);
        setInt64Param(stmt, 2, created_by);
        stmt->setString(3, lines_json);

        // OUT 参数
        stmt->registerOutParam(4, OCCIINT, sizeof(int));   // o_inbound_id
        stmt->registerOutParam(5, OCCIINT, sizeof(int));   // o_result
        stmt->registerOutParam(6, OCCISTRING, MAX_MSG_LEN, "");  // o_message

        stmt->executeUpdate();

        r.inbound_id = safeGetInt(stmt, 4);
        r.code       = safeGetInt(stmt, 5);
        r.message    = safeGetString(stmt, 6);

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;

        // 若存储过程内部异常（code 为负但不等于 -1/-2），code 可能是 ORA 错误码
        // 兼容处理：如果 message 以 "创建失败" 开头且 code 不是 -1/-2，
        // 说明是 EXCEPTION WHEN OTHERS 分支（code = SQLCODE），保留原样
    } catch (const SQLException& e) {
        r.code    = e.getErrorCode();
        r.message = std::string("Oracle 异常 [createInboundOrder]: ") + e.getMessage();
    }

    if (stmt) { pImpl_->conn->terminateStatement(stmt); }
    pImpl_->clearOperatorContext();
    return r;
}

// =========================================================================
// 2.1.2 proc_inbound_submit — 提交入库单
// =========================================================================
DaoResult InventoryDAO::submitInboundOrder(
    int64_t inbound_id,
    int64_t operator_id)
{
    Statement* stmt = nullptr;
    try {
        pImpl_->setOperatorContext(operator_id);

        stmt = pImpl_->conn->createStatement(
            "BEGIN proc_inbound_submit(:1, :2, :3, :4); END;");

        setInt64Param(stmt, 1, inbound_id);
        setInt64Param(stmt, 2, operator_id);
        stmt->registerOutParam(3, OCCIINT, sizeof(int));
        stmt->registerOutParam(4, OCCISTRING, MAX_MSG_LEN, "");

        DaoResult r = executeAndGetResult(stmt, 3, 4);

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;
        pImpl_->clearOperatorContext();

        return r;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        pImpl_->clearOperatorContext();
        return { e.getErrorCode(), std::string("Oracle 异常 [submitInboundOrder]: ") + e.getMessage() };
    }
}

// =========================================================================
// 2.1.3 proc_inbound_receive — 原子化收货确认（核心）
// =========================================================================
ReceiveResult InventoryDAO::receiveInbound(
    int64_t                 line_id,
    double                  receive_quantity,
    int64_t                 operator_id,
    std::optional<int64_t>  expected_version)
{

    ReceiveResult r;
    Statement* stmt = nullptr;

    try {
        pImpl_->setOperatorContext(operator_id);

        stmt = pImpl_->conn->createStatement(
            "BEGIN proc_inbound_receive(:1, :2, :3, :4, :5, :6, :7, :8); END;");

        // IN 参数
        setInt64Param(stmt, 1, line_id);
        stmt->setDouble(2, receive_quantity);
        setInt64Param(stmt, 3, operator_id);
        setOptionalInt(stmt, 4, expected_version);

        // OUT 参数
        stmt->registerOutParam(5, OCCIDOUBLE, sizeof(double));       // o_received_total
        stmt->registerOutParam(6, OCCIINT,    sizeof(int));          // o_new_version
        stmt->registerOutParam(7, OCCIINT,    sizeof(int));          // o_result
        stmt->registerOutParam(8, OCCISTRING, MAX_MSG_LEN, "");     // o_message

        stmt->executeUpdate();

        r.received_total = safeGetDouble(stmt, 5);
        r.new_version    = safeGetInt(stmt, 6);
        r.code           = safeGetInt(stmt, 7);
        r.message        = safeGetString(stmt, 8);

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;
    } catch (const SQLException& e) {
        r.code    = e.getErrorCode();
        r.message = std::string("Oracle 异常 [receiveInbound]: ") + e.getMessage();
    }

    if (stmt) { pImpl_->conn->terminateStatement(stmt); }
    pImpl_->clearOperatorContext();
    return r;
}

// =========================================================================
// 2.1.4 proc_inbound_cancel — 取消入库单
// =========================================================================
DaoResult InventoryDAO::cancelInboundOrder(
    int64_t inbound_id,
    int64_t operator_id)
{

    Statement* stmt = nullptr;
    try {
        pImpl_->setOperatorContext(operator_id);

        stmt = pImpl_->conn->createStatement(
            "BEGIN proc_inbound_cancel(:1, :2, :3, :4); END;");

        setInt64Param(stmt, 1, inbound_id);
        setInt64Param(stmt, 2, operator_id);
        stmt->registerOutParam(3, OCCIINT, sizeof(int));
        stmt->registerOutParam(4, OCCISTRING, MAX_MSG_LEN, "");

        DaoResult r = executeAndGetResult(stmt, 3, 4);

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;
        pImpl_->clearOperatorContext();

        return r;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        pImpl_->clearOperatorContext();
        return { e.getErrorCode(), std::string("Oracle 异常 [cancelInboundOrder]: ") + e.getMessage() };
    }
}

// =========================================================================
// 2.1.5 查询 — getInboundOrder（含明细行）
// =========================================================================
std::optional<InboundOrder> InventoryDAO::getInboundOrder(int64_t inbound_id) {
    InboundOrder order;
    Statement* stmt = nullptr;
    ResultSet* rs   = nullptr;
    bool found = false;

    try {
        // ----- 查询主记录 -----
        stmt = pImpl_->conn->createStatement(
            "SELECT inbound_id, supplier_id, status, created_by, "
            "       TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS'), "
            "       TO_CHAR(received_at, 'YYYY-MM-DD HH24:MI:SS'), "
            "       NVL(remark, '') "
            "FROM inbound_orders WHERE inbound_id = :1");
        setInt64Param(stmt, 1, inbound_id);
        rs = stmt->executeQuery();

        if (rs->next()) {
            found = true;
            order.inbound_id  = rs->getInt(1);
            order.supplier_id = rs->getInt(2);
            order.status      = rs->getString(3);
            order.created_by  = rs->getInt(4);
            order.created_at  = rs->getString(5);
            order.received_at = rs->getString(6);
            order.remark      = rs->getString(7);
        }

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;

        if (!found) return std::nullopt;

        // ----- 查询明细行 -----
        stmt = pImpl_->conn->createStatement(
            "SELECT line_id, inbound_id, product_id, "
            "       quantity_ordered, quantity_received, "
            "       NVL(unit_price, 0) "
            "FROM inbound_order_lines "
            "WHERE inbound_id = :1 ORDER BY line_id");
        setInt64Param(stmt, 1, inbound_id);
        rs = stmt->executeQuery();

        while (rs->next()) {
            InboundOrderLine line;
            line.line_id          = rs->getInt(1);
            line.inbound_id       = rs->getInt(2);
            line.product_id       = rs->getInt(3);
            line.quantity_ordered = rs->getDouble(4);
            line.quantity_received = rs->getDouble(5);
            line.unit_price       = rs->getDouble(6);
            order.lines.push_back(line);
        }

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;

        return order;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        // 查询失败返回 nullopt，由上层处理
        return std::nullopt;
    }
}

// =========================================================================
// 2.1.6 查询 — listInboundOrders（分页 + 筛选）
// =========================================================================
std::vector<InboundOrder> InventoryDAO::listInboundOrders(
    const std::string&  status_filter,
    int64_t             supplier_id,
    int                 limit,
    int                 offset)
{
    std::vector<InboundOrder> orders;
    Statement* stmt = nullptr;
    ResultSet* rs   = nullptr;

    try {
        // 构建动态 SQL
        std::ostringstream sql;
        sql << "SELECT inbound_id, supplier_id, status, created_by, "
            << "       TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS'), "
            << "       TO_CHAR(received_at, 'YYYY-MM-DD HH24:MI:SS'), "
            << "       NVL(remark, '') "
            << "FROM inbound_orders WHERE 1=1 ";

        int param_idx = 0;
        if (!status_filter.empty()) {
            sql << "AND status = :" << (++param_idx) << " ";
        }
        if (supplier_id > 0) {
            sql << "AND supplier_id = :" << (++param_idx) << " ";
        }

        sql << "ORDER BY inbound_id DESC "
            << "OFFSET :" << (++param_idx) << " ROWS "
            << "FETCH NEXT :" << (++param_idx) << " ROWS ONLY";

        stmt = pImpl_->conn->createStatement(sql.str());

        param_idx = 0;
        if (!status_filter.empty()) {
            stmt->setString(++param_idx, status_filter);
        }
        if (supplier_id > 0) {
            setInt64Param(stmt, ++param_idx, supplier_id);
        }
        stmt->setInt(++param_idx, offset);
        stmt->setInt(++param_idx, limit);

        rs = stmt->executeQuery();

        while (rs->next()) {
            InboundOrder order;
            order.inbound_id   = rs->getInt(1);
            order.supplier_id  = rs->getInt(2);
            order.status       = rs->getString(3);
            order.created_by   = rs->getInt(4);
            order.created_at   = rs->getString(5);
            order.received_at  = rs->getString(6);
            order.remark       = rs->getString(7);
            orders.push_back(order);
        }

        pImpl_->conn->terminateStatement(stmt);
        stmt = nullptr;

        return orders;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        return orders;  // 空列表
    }
}

// =========================================================================
// 2.1.7 查询 — getInventory
// =========================================================================
std::optional<InventoryRecord> InventoryDAO::getInventory(int64_t product_id) {
    Statement* stmt = nullptr;
    ResultSet* rs   = nullptr;

    try {
        stmt = pImpl_->conn->createStatement(
            "SELECT inventory_id, product_id, quantity, version, "
            "       TO_CHAR(updated_at, 'YYYY-MM-DD HH24:MI:SS') "
            "FROM inventory WHERE product_id = :1");
        setInt64Param(stmt, 1, product_id);
        rs = stmt->executeQuery();

        if (rs->next()) {
            InventoryRecord rec;
            rec.inventory_id = rs->getInt(1);
            rec.product_id   = rs->getInt(2);
            rec.quantity     = rs->getDouble(3);
            rec.version      = rs->getInt(4);
            rec.updated_at   = rs->getString(5);

            pImpl_->conn->terminateStatement(stmt);
            return rec;
        }

        pImpl_->conn->terminateStatement(stmt);
        return std::nullopt;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        return std::nullopt;
    }
}

// =========================================================================
// 2.1.8 查询 — listInventory
// =========================================================================
std::vector<InventoryRecord> InventoryDAO::listInventory(int limit, int offset) {
    std::vector<InventoryRecord> records;
    Statement* stmt = nullptr;
    ResultSet* rs   = nullptr;

    try {
        stmt = pImpl_->conn->createStatement(
            "SELECT inventory_id, product_id, quantity, version, "
            "       TO_CHAR(updated_at, 'YYYY-MM-DD HH24:MI:SS') "
            "FROM inventory ORDER BY product_id "
            "OFFSET :1 ROWS FETCH NEXT :2 ROWS ONLY");
        stmt->setInt(1, offset);
        stmt->setInt(2, limit);
        rs = stmt->executeQuery();

        while (rs->next()) {
            InventoryRecord rec;
            rec.inventory_id = rs->getInt(1);
            rec.product_id   = rs->getInt(2);
            rec.quantity     = rs->getDouble(3);
            rec.version      = rs->getInt(4);
            rec.updated_at   = rs->getString(5);
            records.push_back(rec);
        }

        pImpl_->conn->terminateStatement(stmt);
        return records;
    } catch (const SQLException& e) {
        if (stmt) { pImpl_->conn->terminateStatement(stmt); }
        return records;
    }
}

} // namespace dao
} // namespace wms
