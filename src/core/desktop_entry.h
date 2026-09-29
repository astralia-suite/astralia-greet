#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

struct DesktopEntry {
    std::string name;
    std::string comment;
    std::string exec;
    std::string try_exec;
    std::vector<std::string> desktop_names;
    bool hidden = false;
    bool no_display = false;
};

std::optional<DesktopEntry> parse_desktop_entry(std::string_view text);
std::optional<DesktopEntry> load_desktop_entry(const std::filesystem::path &path);
std::string strip_field_codes(std::string_view exec);

} // namespace core
