#pragma once

#include <string>
#include <vector>

namespace mis::services {

class WarehouseService {
public:
    struct WarehouseItem {
        int id{0};
        std::string code;
        std::string name;
        std::string address;
        std::string status;
    };

    std::vector<WarehouseItem> list();
    WarehouseItem getById(int id);
    WarehouseItem getByCode(const std::string& code);
};

WarehouseService makeWarehouseService();

} // namespace mis::services
