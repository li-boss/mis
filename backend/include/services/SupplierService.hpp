#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace mis::services {

class SupplierService {
public:
    struct SupplierItem {
        int id{0};
        std::string supplierCode;
        std::string name;
        std::string contactName;
        std::string phone;
        std::string rating;
        std::string status;           // active / paused
        std::string address;
        std::string remark;
    };

    std::vector<SupplierItem> list(const std::string& keyword = "",
                                   const std::string& rating = "",
                                   const std::string& status = "");

    SupplierItem getById(int id);
    SupplierItem create(const SupplierItem& item);
    SupplierItem update(int id, const SupplierItem& item);
    void remove(int id);
    void updateStatus(int id, const std::string& status);
};

SupplierService makeSupplierService();

} // namespace mis::services
