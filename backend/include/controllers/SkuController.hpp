#pragma once

#include <httplib.h>

namespace mis::controllers {

class SkuController {
public:
    void registerRoutes(httplib::Server& server);
};

void registerSkuRoutes(httplib::Server& server);

} // namespace mis::controllers
