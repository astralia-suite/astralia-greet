#include "service/user_service.h"

#include <algorithm>
#include <array>
#include <pwd.h>

#include "core/file.h"
#include "core/strings.h"

namespace service {

namespace {

std::vector<std::string_view> fields(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        auto end = line.find(':', start);
        if (end == std::string_view::npos) {
            out.push_back(line.substr(start));
            return out;
        }
        out.push_back(line.substr(start, end - start));
        start = end + 1;
    }
}

std::string display_name(std::string_view gecos, std::string_view name) {
    auto full = core::trim(gecos.substr(0, gecos.find(',')));
    return std::string(full.empty() ? name : full);
}

} // namespace

std::vector<User> parse_passwd(std::string_view text) {
    std::vector<User> users;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        auto line = core::trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        auto parts = fields(line);
        if (parts.size() < 7) {
            continue;
        }
        auto uid = core::parse_int(parts[2]);
        auto gid = core::parse_int(parts[3]);
        if (!uid || !gid || *uid < 0 || *gid < 0) {
            continue;
        }
        User user;
        user.name = std::string(parts[0]);
        user.uid = static_cast<unsigned>(*uid);
        user.gid = static_cast<unsigned>(*gid);
        user.display_name = display_name(parts[4], parts[0]);
        user.home = std::string(parts[5]);
        user.shell = std::string(parts[6]);
        users.push_back(std::move(user));
    }
    return users;
}

std::vector<User> filter_users(std::vector<User> users, const config::UserSettings &settings) {
    std::erase_if(users, [&](const User &user) {
        return user.uid < static_cast<unsigned>(settings.minimum_uid) || user.uid > static_cast<unsigned>(settings.maximum_uid) || std::ranges::contains(settings.hide_users, user.name) || std::ranges::contains(settings.hide_shells, user.shell);
    });
    std::ranges::sort(users, {}, &User::name);
    return users;
}

std::filesystem::path find_avatar(const User &user, const std::filesystem::path &icon_dir) {
    std::array<std::filesystem::path, 3> candidates{
        icon_dir / user.name,
        std::filesystem::path(user.home) / ".face.icon",
        std::filesystem::path(user.home) / ".face",
    };
    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void UserService::load(const config::UserSettings &settings, const std::filesystem::path &passwd) {
    users_.clear();
    auto text = core::read_file(passwd);
    if (!text) {
        return;
    }
    users_ = filter_users(parse_passwd(*text), settings);
    for (auto &user : users_) {
        user.avatar = find_avatar(user, config::accounts_icon_dir);
    }
}

const std::vector<User> &UserService::users() const {
    return users_;
}

std::optional<User> UserService::find(std::string_view name) const {
    auto found = std::ranges::find(users_, name, &User::name);
    if (found != users_.end()) {
        return *found;
    }
    std::string key(name);
    passwd entry{};
    passwd *result = nullptr;
    std::array<char, 4096> buffer{};
    if (getpwnam_r(key.c_str(), &entry, buffer.data(), buffer.size(), &result) != 0 || !result) {
        return std::nullopt;
    }
    User user;
    user.name = entry.pw_name;
    user.uid = entry.pw_uid;
    user.gid = entry.pw_gid;
    user.display_name = display_name(entry.pw_gecos ? entry.pw_gecos : "", user.name);
    user.home = entry.pw_dir ? entry.pw_dir : "/";
    user.shell = entry.pw_shell ? entry.pw_shell : "";
    user.avatar = find_avatar(user, config::accounts_icon_dir);
    return user;
}

} // namespace service
