#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/greet_config.h"

namespace service {

struct User {
    std::string name;
    std::string display_name;
    std::string home;
    std::string shell;
    unsigned uid = 0;
    unsigned gid = 0;
    std::filesystem::path avatar;
};

std::vector<User> parse_passwd(std::string_view text);
std::vector<User> filter_users(std::vector<User> users, const config::UserSettings &settings);
std::filesystem::path find_avatar(const User &user, const std::filesystem::path &icon_dir);

class UserService {
  public:
    void load(const config::UserSettings &settings, const std::filesystem::path &passwd = "/etc/passwd");
    const std::vector<User> &users() const;
    std::optional<User> find(std::string_view name) const;

  private:
    std::vector<User> users_;
};

} // namespace service
