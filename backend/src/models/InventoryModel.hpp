#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mis::models {

struct SKUItem {
    std::string id;
    std::string name;
    std::string category;
    std::string unit;
    bool active{true};
};

struct InventoryItem {
    std::string warehouseId{"DEFAULT"};
    std::string skuId;
    int quantity{0};
    std::optional<std::string> updatedBy;
};

struct InboundRequest {
    std::string skuId;
    int quantity{0};
    std::string warehouseId{"DEFAULT"};
    std::optional<std::string> operatorId;
};

struct InventorySummary {
    std::vector<InventoryItem> topItems;
};

class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace mis::models
