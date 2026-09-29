#include "service/session_service.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

#include "core/desktop_entry.h"
#include "core/log.h"
#include "core/process.h"
#include "core/strings.h"

namespace service {

namespace {

std::string_view type_prefix(config::SessionType type) {
    return type == config::SessionType::Wayland ? "wayland" : "x11";
}

std::string_view type_label(config::SessionType type) {
    return type == config::SessionType::Wayland ? "Wayland" : "X11";
}

void scan_dir(const std::filesystem::path &dir, config::SessionType type, std::string_view path_list, std::set<std::string> &seen, std::vector<Session> &out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const auto &item : std::filesystem::directory_iterator(dir, ec)) {
        if (item.path().extension() == ".desktop") {
            files.push_back(item.path());
        }
    }
    std::ranges::sort(files);
    for (const auto &file : files) {
        auto id = std::string(type_prefix(type)) + "/" + file.stem().string();
        if (seen.contains(id)) {
            continue;
        }
        auto entry = core::load_desktop_entry(file);
        if (!entry || entry->hidden || entry->no_display) {
            continue;
        }
        if (!entry->try_exec.empty() && !core::find_executable(entry->try_exec, path_list)) {
            continue;
        }
        seen.insert(id);
        out.push_back({id, entry->name, entry->comment, entry->exec, type, entry->desktop_names});
    }
}

} // namespace

std::vector<Session> detect_sessions(const config::SessionSettings &settings, std::string_view path_list) {
    std::vector<Session> sessions;
    std::set<std::string> seen;
    for (const auto &dir : settings.wayland_dirs) {
        scan_dir(dir, config::SessionType::Wayland, path_list, seen, sessions);
    }
    for (const auto &dir : settings.x11_dirs) {
        scan_dir(dir, config::SessionType::X11, path_list, seen, sessions);
    }
    for (const auto &custom : settings.custom) {
        if (custom.exec.empty()) {
            continue;
        }
        auto id = "custom/" + custom.id;
        if (seen.insert(id).second) {
            sessions.push_back({id, custom.name.empty() ? custom.id : custom.name, {}, custom.exec, custom.type, custom.desktop_names});
        }
    }
    std::map<std::string, int> name_count;
    for (const auto &session : sessions) {
        ++name_count[session.name];
    }
    for (auto &session : sessions) {
        if (name_count[session.name] > 1) {
            session.name += std::string(" (") + std::string(type_label(session.type)) + ")";
        }
    }
    std::ranges::stable_sort(sessions, [](const Session &a, const Session &b) { return core::lowercase(a.name) < core::lowercase(b.name); });
    return sessions;
}

std::string session_desktop_name(const Session &session) {
    auto slash = session.id.find('/');
    return core::lowercase(slash == std::string::npos ? session.id : session.id.substr(slash + 1));
}

void SessionService::load(const config::SessionSettings &settings) {
    sessions_ = detect_sessions(settings, config::default_path);
    core::info("detected {} sessions", sessions_.size());
}

const std::vector<Session> &SessionService::sessions() const {
    return sessions_;
}

std::optional<std::size_t> SessionService::index_of(std::string_view id_or_name) const {
    for (std::size_t i = 0; i < sessions_.size(); ++i) {
        if (sessions_[i].id == id_or_name || sessions_[i].name == id_or_name) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace service
