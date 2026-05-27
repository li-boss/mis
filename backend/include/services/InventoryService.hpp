#pragma once

#include "models/InventoryModel.hpp"

namespace mis::services {

class InventoryService {
public:
    void submitInbound(const models::InboundRequest& request);

private:
    static void validateInbound(const models::InboundRequest& request);
};

InventoryService makeInventoryService();

} // namespace mis::services
