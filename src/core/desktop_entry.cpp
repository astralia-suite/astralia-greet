#include "core/desktop_entry.h"

#include "core/file.h"
#include "core/ini.h"
#include "core/strings.h"

namespace core {

std::optional<DesktopEntry> parse_desktop_entry(std::string_view text) {
    constexpr std::string_view group = "Desktop Entry";
    auto ini = parse_ini(text);
    if (!ini.has(group)) {
        return std::nullopt;
    }
    DesktopEntry entry;
    entry.name = ini.get_or(group, "Name", "");
    entry.exec = strip_field_codes(ini.get_or(group, "Exec", ""));
    if (entry.name.empty() || entry.exec.empty()) {
        return std::nullopt;
    }
    entry.comment = ini.get_or(group, "Comment", "");
    entry.try_exec = ini.get_or(group, "TryExec", "");
    entry.desktop_names = ini.get_list(group, "DesktopNames").value_or(std::vector<std::string>{});
    entry.hidden = ini.get_bool(group, "Hidden", false);
    entry.no_display = ini.get_bool(group, "NoDisplay", false);
    return entry;
}

std::optional<DesktopEntry> load_desktop_entry(const std::filesystem::path &path) {
    auto text = read_file(path);
    if (!text) {
        return std::nullopt;
    }
    return parse_desktop_entry(*text);
}

std::string strip_field_codes(std::string_view exec) {
    std::string out;
    for (std::size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] == '%' && i + 1 < exec.size()) {
            if (exec[i + 1] == '%') {
                out += '%';
            }
            ++i;
            continue;
        }
        out += exec[i];
    }
    return join(split_words(out), " ");
}

} // namespace core
