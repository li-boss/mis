#include "dao/InventoryDAO.hpp"
#include "dao/OracleConnector.hpp"

#include <unordered_map>

namespace mis::dao {

void InventoryDAO::inbound(const models::InboundRequest& request)
{
    std::unordered_map<std::string, std::string> binds{
        {"sku_id", request.skuId},
        {"quantity", std::to_string(request.quantity)},
        {"warehouse_id", request.warehouseId}
    };

    if (request.operatorId.has_value()) {
        binds.emplace("operator_id", *request.operatorId);
    }

    oracle().callProcedure("proc_inbound", binds);
}

bool InventoryDAO::skuExists(const std::string& skuId)
{
    (void)skuId;
    // TODO: Query SKU where sku_id = :sku_id and is_active = 1.
    return true;
}

InventoryDAO makeInventoryDAO()
{
    return InventoryDAO{};
}

} // namespace mis::dao
