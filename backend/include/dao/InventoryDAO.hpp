// =============================================================================
// WMS — 库存数据访问对象头文件
//
// 职责：
//   - 封装 Oracle 对 inventory / inbound_orders / inbound_order_lines 的访问
//   - 调用 proc_inbound_* 系列存储过程完成入库操作
//   - 由 李佳恒 的 InventoryService 层调用
//
// 依赖：
//   - Oracle OCCI (Oracle C++ Call Interface)
//   - cpp-httplib（通过 Service 层间接使用）
//
// 协作契约（与 李佳恒 / 白沁禾）：
//   - DAO 仅负责数据库操作，不处理 HTTP 序列化
//   - Service 层负责参数校验、JSON 解析/序列化、调用 DAO
//   - Controller 层（cpp-httplib handler）将 HTTP Request → Service 参数
// =============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wms {
namespace dao {

// =========================================================================
// 数据载体结构体（POD，对应数据库表）
// =========================================================================

/// 入库单明细行
struct InboundOrderLine {
    int64_t line_id          = 0;
    int64_t inbound_id       = 0;
    int64_t product_id       = 0;
    double  quantity_ordered = 0.0;
    double  quantity_received = 0.0;
    double  unit_price       = 0.0;
};

/// 入库单主记录（含明细行）
struct InboundOrder {
    int64_t  inbound_id  = 0;
    int64_t  supplier_id = 0;
    std::string status;          // DRAFT / SUBMITTED / PARTIAL / RECEIVED / CANCELLED
    int64_t  created_by  = 0;
    std::string created_at;
    std::string received_at;
    std::string remark;
    std::vector<InboundOrderLine> lines;
};

/// 库存记录
struct InventoryRecord {
    int64_t inventory_id = 0;
    int64_t product_id   = 0;
    double  quantity     = 0.0;
    int64_t version      = 0;
    std::string updated_at;
};

// =========================================================================
// 操作结果类型（与存储过程返回码对应）
// =========================================================================

/// 通用结果（code=0 成功，负值对应存储过程错误码）
struct DaoResult {
    int         code = 0;
    std::string message;
    bool ok() const { return code == 0; }
};

/// 创建入库单结果
struct CreateResult : public DaoResult {
    int64_t inbound_id = 0;
};

/// 收货结果
struct ReceiveResult : public DaoResult {
    double  received_total = 0.0;
    int64_t new_version    = 0;
};

// =========================================================================
// 存储过程错误码常量（与 proc_inbound.sql 同步）
// =========================================================================
namespace ErrorCode {
    // proc_inbound_create
    constexpr int SUPPLIER_ID_NULL   = -1;
    constexpr int LINES_JSON_EMPTY   = -2;

    // proc_inbound_receive
    constexpr int LINE_NOT_FOUND     = -100;
    constexpr int ORDER_CANCELLED    = -101;
    constexpr int ORDER_RECEIVED     = -102;
    constexpr int QUANTITY_EXCEED    = -103;
    constexpr int QUANTITY_NEGATIVE  = -104;
    constexpr int VERSION_CONFLICT   = -200;

    // proc_inbound_cancel
    constexpr int CANCEL_NOT_FOUND   = -300;
    constexpr int CANCEL_ALREADY     = -301;
    constexpr int CANCEL_RECEIVED    = -302;

    // proc_inbound_submit
    constexpr int SUBMIT_NOT_DRAFT   = -400;
}

// =========================================================================
// 抽象 DAO 接口（方便 Mock 和单元测试）
// =========================================================================
class IInventoryDAO {
public:
    virtual ~IInventoryDAO() = default;

    // ---- 入库操作 ----

    /// 创建采购入库单（含明细行 JSON）
    /// @param supplier_id  供应商 ID
    /// @param created_by   创建人用户 ID
    /// @param lines_json   JSON 数组 [{"product_id":, "quantity":, "unit_price":}, ...]
    virtual CreateResult createInboundOrder(
        int64_t             supplier_id,
        int64_t             created_by,
        const std::string&  lines_json) = 0;

    /// 提交入库单（DRAFT → SUBMITTED）
    virtual DaoResult submitInboundOrder(
        int64_t inbound_id,
        int64_t operator_id) = 0;

    /// 原子化收货确认（单明细行）
    /// @param line_id           入库明细行 ID
    /// @param receive_quantity  本次收货数量
    /// @param operator_id       操作人 ID
    /// @param expected_version  乐观锁版本号（首次收货传 nullopt）
    virtual ReceiveResult receiveInbound(
        int64_t line_id,
        double  receive_quantity,
        int64_t operator_id,
        std::optional<int64_t> expected_version = std::nullopt) = 0;

    /// 取消入库单（自动回退已收库存）
    virtual DaoResult cancelInboundOrder(
        int64_t inbound_id,
        int64_t operator_id) = 0;

    // ---- 查询操作 ----

    /// 按 ID 查询入库单（含明细行）
    virtual std::optional<InboundOrder> getInboundOrder(int64_t inbound_id) = 0;

    /// 列表查询入库单
    /// @param status_filter 状态筛选（空字符串 = 全部）
    /// @param supplier_id   供应商筛选（0 = 全部）
    virtual std::vector<InboundOrder> listInboundOrders(
        const std::string& status_filter = "",
        int64_t             supplier_id   = 0,
        int                 limit         = 50,
        int                 offset        = 0) = 0;

    /// 按 product_id 查库存
    virtual std::optional<InventoryRecord> getInventory(int64_t product_id) = 0;

    /// 库存列表（支持分页）
    virtual std::vector<InventoryRecord> listInventory(
        int  limit  = 50,
        int  offset = 0) = 0;
};

// =========================================================================
// OCCI 实现（生产环境）
// =========================================================================
class InventoryDAO : public IInventoryDAO {
public:
    /// @param conn_str  Oracle Easy Connect 字符串，如 "localhost:1521/XEPDB1"
    /// @param user      Oracle 用户名
    /// @param password  Oracle 密码
    InventoryDAO(const std::string& conn_str,
                 const std::string& user,
                 const std::string& password);
    ~InventoryDAO() override;

    // 禁止拷贝
    InventoryDAO(const InventoryDAO&) = delete;
    InventoryDAO& operator=(const InventoryDAO&) = delete;

    // 允许移动
    InventoryDAO(InventoryDAO&&) noexcept;
    InventoryDAO& operator=(InventoryDAO&&) noexcept;

    // ---- IInventoryDAO 接口实现 ----
    CreateResult createInboundOrder(
        int64_t supplier_id, int64_t created_by,
        const std::string& lines_json) override;

    DaoResult submitInboundOrder(
        int64_t inbound_id, int64_t operator_id) override;

    ReceiveResult receiveInbound(
        int64_t line_id, double receive_quantity,
        int64_t operator_id,
        std::optional<int64_t> expected_version = std::nullopt) override;

    DaoResult cancelInboundOrder(
        int64_t inbound_id, int64_t operator_id) override;

    std::optional<InboundOrder> getInboundOrder(int64_t inbound_id) override;

    std::vector<InboundOrder> listInboundOrders(
        const std::string& status_filter, int64_t supplier_id,
        int limit, int offset) override;

    std::optional<InventoryRecord> getInventory(int64_t product_id) override;

    std::vector<InventoryRecord> listInventory(int limit, int offset) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace dao
} // namespace wms
