#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "service/session_service.h"
#include "service/user_service.h"

namespace app {

enum class Focus { Password,
                   Session };

enum class View { Clock,
                  Login };

enum class Request { None,
                     Login,
                     Reboot,
                     PowerOff,
                     Suspend };

struct GreeterState {
    std::vector<service::User> users;
    std::size_t user_index = 0;
    std::string password;
    std::vector<service::Session> sessions;
    std::size_t session_index = 0;
    bool session_menu_open = false;
    bool user_menu_open = false;
    View view = View::Clock;
    Focus focus = Focus::Password;
    std::string message;
    bool message_is_error = false;
    bool busy = false;
    bool caps_lock = false;
    std::string layout;
    Request request = Request::None;
};

} // namespace app
