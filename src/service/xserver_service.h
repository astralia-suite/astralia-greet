#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

#include "config/greet_config.h"

#include "core/process.h"

namespace service {

struct XServer {
    pid_t pid = 0;
    int display = 0;
    std::filesystem::path server_auth;
    std::filesystem::path user_auth;
};

std::vector<std::uint8_t> xauth_entry(std::string_view display, std::span<const std::uint8_t> cookie);
std::optional<int> find_free_display(const std::filesystem::path &tmp_dir);
std::filesystem::path user_auth_path(const std::filesystem::path &dir, unsigned uid, int display);
std::vector<std::string> xorg_command(const config::XorgSettings &settings, int display, int vt, const std::filesystem::path &auth);
std::vector<std::vector<std::string>> xrdb_commands(const std::filesystem::path &system_file, const std::filesystem::path &home);

class XServerService {
  public:
    std::optional<XServer> start(const config::XorgSettings &settings, int vt, const std::filesystem::path &user_auth_dir, unsigned uid, unsigned gid);
    void run_display_setup(const std::string &command, const XServer &server);
    static void merge_resources(const std::filesystem::path &home, const core::Environment &env);
    void stop(XServer &server);
};

} // namespace service
