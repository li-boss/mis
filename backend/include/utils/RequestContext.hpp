#pragma once

#include <string>

namespace mis::context {

struct RequestContext {
    int userId = 0;
    std::string username;
    std::string role;
};

// Thread-local request context to propagate authenticated user info
extern thread_local RequestContext currentContext;

} // namespace mis::context
