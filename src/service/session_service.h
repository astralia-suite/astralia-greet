#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/greet_config.h"

namespace service {

struct Session {
    std::string id;
    std::string name;
    std::string comment;
    std::string exec;
    config::SessionType type = config::SessionType::X11;
    std::vector<std::string> desktop_names;
};

std::vector<Session> detect_sessions(const config::SessionSettings &settings, std::string_view path_list);
std::string session_desktop_name(const Session &session);

class SessionService {
  public:
    void load(const config::SessionSettings &settings);
    const std::vector<Session> &sessions() const;
    std::optional<std::size_t> index_of(std::string_view id_or_name) const;

  private:
    std::vector<Session> sessions_;
};

} // namespace service
