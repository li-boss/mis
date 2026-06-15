#pragma once
#include "models/InventoryModel.hpp"
#include <vector>
#include <utility>
#include <string>

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
        std::vector<std::pair<std::string, int>> trend;
    };
    DashboardStats getDashboard();

private:
    static void validateInbound(const models::InboundRequest& request);
};

InventoryService makeInventoryService();

} // namespace mis::services
