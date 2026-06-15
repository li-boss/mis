#pragma once

#include <httplib.h>

namespace mis::controllers {

class AuthController {
public:
    void registerRoutes(httplib::Server& server);
};

void registerAuthRoutes(httplib::Server& server);

} // namespace mis::controllers
