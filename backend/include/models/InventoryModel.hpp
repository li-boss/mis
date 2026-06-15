#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mis::models {

// ---- 入库明细行 ----
struct InboundLine {
    int lineId{0};
    int productId{0};
    int quantityOrdered{0};
    int quantityReceived{0};
    double unitPrice{0.0};
};

// ---- 入库单 ----
struct InboundOrder {
    int inboundId{0};
    int supplierId{0};
    std::string status{"DRAFT"};   // DRAFT / SUBMITTED / PARTIAL / RECEIVED / CANCELLED
    int createdBy{0};
    std::string createdAt;
    std::string receivedAt;
    std::string remark;
    std::vector<InboundLine> lines;
};

// ---- 入库请求（创建） ----
struct InboundRequest {
    int supplierId{0};
    int createdBy{0};
    std::string remark;
    std::vector<InboundLine> lines;
};

// ---- 收货请求 ----
struct ReceiveRequest {
    int lineId{0};
    double receiveQuantity{0};
    int operatorId{0};
    std::optional<int> expectedVersion;
};

// ---- SKU 模型 ----
struct SKUItem {
    std::string id;
    std::string name;
    std::string category;
    std::string unit;
    bool active{true};
};

// ---- 库存记录 ----
struct InventoryItem {
    std::string warehouseId{"DEFAULT"};
    std::string skuId;
    int quantity{0};
    std::optional<std::string> updatedBy;
};

// ---- 库存汇总 ----
struct InventorySummary {
    std::vector<InventoryItem> topItems;
};

// ---- 异常 ----
class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace mis::models
