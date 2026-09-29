#pragma once

#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

#include "config/greet_config.h"

#include "core/process.h"

#include "service/auth_service.h"
#include "service/session_service.h"
#include "service/user_service.h"

namespace service {

struct LaunchRequest {
    const User &user;
    const Session &session;
    int vt = 0;
    const config::Settings &settings;
    bool switch_vt = false;
};

core::Environment session_environment(const User &user, const Session &session, int vt);
std::vector<std::string> session_command(const Session &session, const User &user, const config::Settings &settings);

class LaunchService {
  public:
    std::optional<pid_t> launch(AuthService &auth, const LaunchRequest &request);
};

} // namespace service
