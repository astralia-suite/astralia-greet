#include "service/launch_service.h"

#include <array>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <grp.h>
#include <unistd.h>

#include "core/log.h"
#include "core/strings.h"

#include "service/vt_service.h"
#include "service/xserver_service.h"

namespace service {

namespace {

void redirect_output(const std::string &home, bool keep_stdin) {
    auto log = std::filesystem::path(home) / config::session_log;
    std::error_code ec;
    std::filesystem::create_directories(log.parent_path(), ec);
    int null = keep_stdin ? -1 : ::open("/dev/null", O_RDONLY);
    if (null >= 0 && null != STDIN_FILENO) {
        dup2(null, STDIN_FILENO);
        ::close(null);
    }
    int fd = ::open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        ::close(fd);
    }
}

[[noreturn]] void run_user(const LaunchRequest &request, const core::Environment &env, VtService &vt) {
    core::reset_signal_state();
    const auto &user = request.user;
    bool own_vt = request.switch_vt && request.session.type == config::SessionType::Wayland;
    if (own_vt && !vt.take_as_stdin()) {
        _exit(1);
    }
    if (initgroups(user.name.c_str(), user.gid) < 0 || setgid(user.gid) < 0 || setuid(user.uid) < 0) {
        _exit(1);
    }
    if (chdir(user.home.c_str()) < 0 && chdir("/") < 0) {
        _exit(1);
    }
    redirect_output(user.home, own_vt);
    if (request.session.type == config::SessionType::X11) {
        XServerService::merge_resources(user.home, env);
    }
    core::exec_command(session_command(request.session, user, request.settings), env);
}

[[noreturn]] void run_worker(AuthService &auth, const LaunchRequest &request) {
    core::reset_signal_state();
    setsid();
    for (int signal : {SIGTERM, SIGHUP, SIGINT}) {
        std::signal(signal, SIG_IGN);
    }
    VtService vt;
    if (request.switch_vt) {
        if (!vt.open(request.vt)) {
            auth.end(false);
            _exit(1);
        }
        vt.hide_text();
        if (!vt.activate()) {
            vt.show_text();
            auth.end(false);
            _exit(1);
        }
    }
    auto env = session_environment(request.user, request.session, request.vt);
    constexpr std::array<const char *, 5> pam_keys{"XDG_SESSION_TYPE", "XDG_SESSION_CLASS", "XDG_SESSION_DESKTOP", "XDG_SEAT", "XDG_VTNR"};
    for (const char *key : pam_keys) {
        if (auto value = env.get(key)) {
            auth.put_env(key, *value);
        }
    }
    if (!auth.open_session()) {
        vt.show_text();
        auth.end(false);
        _exit(1);
    }
    for (const auto &entry : auth.environment()) {
        env.set_entry(entry);
    }
    XServerService xorg;
    std::optional<XServer> server;
    if (request.session.type == config::SessionType::X11) {
        auto runtime = env.get("XDG_RUNTIME_DIR");
        auto user_auth_dir = runtime ? std::filesystem::path(*runtime) : std::filesystem::path("/tmp");
        server = xorg.start(request.settings.xorg, request.vt, user_auth_dir, request.user.uid, request.user.gid);
        if (!server) {
            vt.show_text();
            auth.close_session();
            auth.end(false);
            _exit(1);
        }
        env.set("DISPLAY", std::format(":{}", server->display));
        env.set("XAUTHORITY", server->user_auth.string());
        xorg.run_display_setup(request.settings.xorg.display_setup, *server);
    }
    pid_t child = fork();
    if (child == 0) {
        run_user(request, env, vt);
    }
    if (child > 0) {
        int status = core::wait_for(child);
        core::info("session for {} exited with status {}", request.user.name, status);
    }
    if (server) {
        xorg.stop(*server);
    }
    vt.show_text();
    auth.close_session();
    auth.end(false);
    _exit(0);
}

} // namespace

core::Environment session_environment(const User &user, const Session &session, int vt) {
    core::Environment env;
    env.set("HOME", user.home);
    env.set("USER", user.name);
    env.set("LOGNAME", user.name);
    env.set("SHELL", user.shell.empty() ? "/bin/sh" : user.shell);
    env.set("PATH", config::default_path);
    env.set("XDG_SEAT", config::seat);
    env.set("XDG_VTNR", std::to_string(vt));
    env.set("XDG_SESSION_CLASS", "user");
    env.set("XDG_SESSION_TYPE", session.type == config::SessionType::Wayland ? "wayland" : "x11");
    env.set("XDG_SESSION_DESKTOP", session_desktop_name(session));
    if (!session.desktop_names.empty()) {
        env.set("XDG_CURRENT_DESKTOP", core::join(session.desktop_names, ":"));
    }
    if (const char *lang = std::getenv("LANG")) {
        env.set("LANG", lang);
    }
    return env;
}

std::vector<std::string> session_command(const Session &session, const User &user, const config::Settings &settings) {
    const auto &wrapper = session.type == config::SessionType::X11 ? settings.xorg.session_wrapper : settings.wayland.session_wrapper;
    if (!wrapper.empty()) {
        auto argv = core::split_words(wrapper);
        argv.push_back(session.exec);
        return argv;
    }
    return {user.shell.empty() ? "/bin/sh" : user.shell, "-l", "-c", "exec " + session.exec};
}

std::optional<pid_t> LaunchService::launch(AuthService &auth, const LaunchRequest &request) {
    pid_t pid = fork();
    if (pid < 0) {
        core::error("fork failed");
        return std::nullopt;
    }
    if (pid == 0) {
        run_worker(auth, request);
    }
    return pid;
}

} // namespace service
