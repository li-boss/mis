#pragma once

#include <httplib.h>

namespace mis::controllers {

class InventoryController {
public:
    void registerRoutes(httplib::Server& server);
};

void registerInventoryRoutes(httplib::Server& server);

} // namespace mis::controllers
