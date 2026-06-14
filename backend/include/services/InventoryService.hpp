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

    // 看板统计
    struct DashboardStats {
        int totalStock{0};
        int inboundToday{0};
        int lowStockSku{0};
        int exceptionCount{0};
        int pendingInbound{0};
    };
    DashboardStats getDashboard();

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
