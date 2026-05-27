#pragma once

#include "models/InventoryModel.hpp"

#include <string>

namespace mis::dao {

class InventoryDAO {
public:
    void inbound(const models::InboundRequest& request);
    bool skuExists(const std::string& skuId);
};

InventoryDAO makeInventoryDAO();

} // namespace mis::dao
