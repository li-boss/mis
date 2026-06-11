#include "dao/InventoryDAO.hpp"
#include "dao/OracleConnector.hpp"

#include <unordered_map>
#include <string>

namespace mis::dao {

void InventoryDAO::inbound(const models::InboundRequest& request)
{
    std::unordered_map<std::string, std::string> binds{
        {"p_sku_id", request.skuId},
        {"p_quantity", std::to_string(request.quantity)},
        {"p_warehouse_id", request.warehouseId}
    };

    if (request.operatorId.has_value() && !request.operatorId->empty()) {
        binds.emplace("p_operator_id", *request.operatorId);
    }

    oracle().callProcedure("proc_inbound", binds);
}

bool InventoryDAO::skuExists(const std::string& skuId)
{
    std::unordered_map<std::string, std::string> binds{
        {"sku_id", skuId}
    };

    auto results = oracle().query(
        "SELECT COUNT(*) AS CNT FROM SKU WHERE sku_id = :sku_id AND is_active = 1",
        binds
    );

    if (!results.empty()) {
        try {
            int count = std::stoi(results[0].at("CNT"));
            return count > 0;
        } catch (...) {
            return false;
        }
    }
    return false;
}

InventoryDAO makeInventoryDAO()
{
    return InventoryDAO{};
}

} // namespace mis::dao
