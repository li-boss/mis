#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace mis::services {

class SkuService {
public:
    struct SkuItem {
        int id{0};
        std::string skuCode;
        std::string name;
        std::string category;
        std::string unit;
        std::string supplierName;    // 暂不连表，返回空
        int currentStock{0};
        int safetyStock{0};          // 暂未实现，返回 0
        std::string status;          // active / disabled
        std::string updatedAt;
    };

    // 列表（支持 keyword / category / status / warehouseId 筛选）
    std::vector<SkuItem> list(const std::string& keyword = "",
                              const std::string& category = "",
                              const std::string& status = "",
                              int warehouseId = 1,
                              bool lowStockOnly = false);

    // 详情
    SkuItem getById(int id, int warehouseId = 1);

    // 创建
    SkuItem create(const SkuItem& item);

    // 更新
    SkuItem update(int id, const SkuItem& item);

    // 删除
    void remove(int id);

    // 状态切换
    void updateStatus(int id, const std::string& status);
};

SkuService makeSkuService();

} // namespace mis::services
