#pragma once

#include <httplib.h>

namespace mis::controllers {

class SupplierController {
public:
    void registerRoutes(httplib::Server& server);
};

void registerSupplierRoutes(httplib::Server& server);

} // namespace mis::controllers
