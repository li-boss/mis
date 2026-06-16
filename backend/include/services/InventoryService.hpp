#pragma once

#include "models/InventoryModel.hpp"

namespace mis::services {

class InventoryService {
public:
    // 入库单操作
    models::InboundOrder createInbound(const models::InboundRequest& request);
    void submitInbound(int inboundId, int operatorId);
    void cancelInbound(int inboundId, int operatorId);
    models::InboundOrder getInbound(int inboundId);
    std::vector<models::InboundOrder> listInbound(
        const std::string& status = "",
        int supplierId = 0,
        int limit = 50,
        int offset = 0);
    void receiveLine(int lineId, double quantity, int operatorId);

    // SKU 收货（快速入库）：根据 SKU 码/productId 自动匹配待收货明细行
    struct ReceiveBySkuResult {
        bool success{false};
        std::string message;
        int totalReceived{0};
        std::vector<int> orderIds;
    };
    ReceiveBySkuResult receiveBySku(const std::string& skuCode, int productId, int quantity);

    // 看板统计
    struct DashboardStats {
        int totalStock{0};
        int previousTotalStock{0};   // 前一日总库存，用于计算变化百分比
        int inboundToday{0};
        int lowStockSku{0};
        int exceptionCount{0};
        int pendingInbound{0};
    };
    DashboardStats getDashboard(int warehouseId = 1);

    // 最近 N 天入库趋势：[{date, quantity}, ...]
    struct TrendPoint { std::string date; int quantity{0}; };
    std::vector<TrendPoint> getRecentTrend(int days = 7, int warehouseId = 1);

private:
    static void validateInbound(const models::InboundRequest& request);

    // ---- 独立模式（内存存储） ----
    std::vector<models::InboundOrder> memOrders_;
    int memNextInboundId_{1};
    int memNextLineId_{1};
    bool useMemoryStore_{true}; // 当 Oracle 不可用时自动降级
};

InventoryService makeInventoryService();

} // namespace mis::services
