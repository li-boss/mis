#include "dao/InventoryDAO.hpp"
#include "dao/OracleConnector.hpp"
#include "services/InventoryService.hpp"

namespace mis::services {

void InventoryService::submitInbound(const models::InboundRequest& request)
{
    validateInbound(request);

    auto dao = dao::makeInventoryDAO();
    if (!dao.skuExists(request.skuId)) {
        throw models::ValidationError("SKU does not exist or is inactive");
    }

    try {
        dao.inbound(request);
        dao::oracle().commit();
    } catch (...) {
        dao::oracle().rollback();
        throw;
    }
}

void InventoryService::validateInbound(const models::InboundRequest& request)
{
    if (request.skuId.empty()) {
        throw models::ValidationError("skuId is required");
    }

    if (request.quantity <= 0) {
        throw models::ValidationError("quantity must be greater than zero");
    }

    if (request.warehouseId.empty()) {
        throw models::ValidationError("warehouseId is required");
    }
}

InventoryService makeInventoryService()
{
    return InventoryService{};
}

} // namespace mis::services
