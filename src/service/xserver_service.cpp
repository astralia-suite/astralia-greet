#include "service/xserver_service.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <format>
#include <sys/random.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "core/file.h"
#include "core/log.h"
#include "core/process.h"

namespace service {

namespace {

constexpr std::uint16_t family_wild = 0xFFFF;

std::array<std::uint8_t, 16> generate_cookie() {
    std::array<std::uint8_t, 16> cookie{};
    std::size_t filled = 0;
    while (filled < cookie.size()) {
        auto n = getrandom(cookie.data() + filled, cookie.size() - filled, 0);
        if (n <= 0) {
            break;
        }
        filled += static_cast<std::size_t>(n);
    }
    return cookie;
}

std::string_view as_text(const std::vector<std::uint8_t> &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

bool wait_ready(pid_t pid, const sigset_t &signals) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(config::xorg_ready_timeout_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        timespec timeout{1, 0};
        siginfo_t info{};
        int signal = sigtimedwait(&signals, &info, &timeout);
        if (signal == SIGUSR1) {
            return true;
        }
        if (signal == SIGCHLD && waitpid(pid, nullptr, WNOHANG) == pid) {
            return false;
        }
    }
    return false;
}

} // namespace

std::vector<std::uint8_t> xauth_entry(std::string_view display, std::span<const std::uint8_t> cookie) {
    std::vector<std::uint8_t> out;
    auto put16 = [&](std::size_t value) {
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    };
    auto put_text = [&](std::string_view text) {
        put16(text.size());
        out.insert(out.end(), text.begin(), text.end());
    };
    put16(family_wild);
    put_text("");
    put_text(display);
    put_text("MIT-MAGIC-COOKIE-1");
    put16(cookie.size());
    out.insert(out.end(), cookie.begin(), cookie.end());
    return out;
}

std::optional<int> find_free_display(const std::filesystem::path &tmp_dir) {
    for (int display = 0; display < 64; ++display) {
        std::error_code ec;
        auto lock = tmp_dir / std::format(".X{}-lock", display);
        auto socket = tmp_dir / ".X11-unix" / std::format("X{}", display);
        if (!std::filesystem::exists(lock, ec) && !std::filesystem::exists(socket, ec)) {
            return display;
        }
    }
    return std::nullopt;
}

std::vector<std::string> xorg_command(const config::XorgSettings &settings, int display, int vt, const std::filesystem::path &auth) {
    std::vector<std::string> argv{settings.path, std::format(":{}", display), std::format("vt{}", vt), "-auth", auth.string()};
    argv.insert(argv.end(), settings.args.begin(), settings.args.end());
    return argv;
}

std::vector<std::vector<std::string>> xrdb_commands(const std::filesystem::path &system_file, const std::filesystem::path &home) {
    std::vector<std::vector<std::string>> commands;
    for (const auto &file : {system_file, home / config::user_xresources}) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(file, ec)) {
            commands.push_back({config::xrdb_path, "-merge", file.string()});
        }
    }
    return commands;
}

std::filesystem::path user_auth_path(const std::filesystem::path &dir, unsigned uid, int display) {
    return dir / std::format("xauth_astralia_{}-{}", uid, display);
}

std::optional<XServer> XServerService::start(const config::XorgSettings &settings, int vt, const std::filesystem::path &user_auth_dir, unsigned uid, unsigned gid) {
    auto display = find_free_display("/tmp");
    if (!display) {
        core::error("no free X display number");
        return std::nullopt;
    }
    std::error_code ec;
    std::filesystem::create_directories(config::runtime_dir, ec);
    XServer server;
    server.display = *display;
    server.server_auth = std::filesystem::path(config::runtime_dir) / std::format("xauth-{}", *display);
    server.user_auth = user_auth_path(user_auth_dir, uid, *display);
    auto cookie = generate_cookie();
    auto entry = xauth_entry(std::to_string(*display), cookie);
    if (!core::write_file(server.server_auth, as_text(entry), 0600) || !core::write_file(server.user_auth, as_text(entry), 0600)) {
        core::error("cannot write X authority files");
        return std::nullopt;
    }
    if (chown(server.user_auth.c_str(), uid, gid) < 0) {
        core::warn("cannot chown {}", server.user_auth.string());
    }
    sigset_t signals;
    sigset_t previous;
    sigemptyset(&signals);
    sigaddset(&signals, SIGUSR1);
    sigaddset(&signals, SIGCHLD);
    sigprocmask(SIG_BLOCK, &signals, &previous);
    core::Environment env;
    env.set("PATH", config::default_path);
    auto argv = xorg_command(settings, *display, vt, server.server_auth);
    server.pid = fork();
    if (server.pid == 0) {
        core::reset_signal_state();
        std::signal(SIGUSR1, SIG_IGN);
        core::exec_command(argv, env);
    }
    bool ready = server.pid > 0 && wait_ready(server.pid, signals);
    sigprocmask(SIG_SETMASK, &previous, nullptr);
    if (!ready) {
        core::error("Xorg on :{} failed to start", *display);
        stop(server);
        return std::nullopt;
    }
    core::info("Xorg ready on :{} vt{}", *display, vt);
    return server;
}

void XServerService::run_display_setup(const std::string &command, const XServer &server) {
    if (command.empty()) {
        return;
    }
    core::Environment env;
    env.set("PATH", config::default_path);
    env.set("DISPLAY", std::format(":{}", server.display));
    env.set("XAUTHORITY", server.server_auth.string());
    pid_t pid = core::spawn({"/bin/sh", "-c", command}, env);
    if (pid > 0) {
        core::wait_for(pid);
    }
}

void XServerService::merge_resources(const std::filesystem::path &home, const core::Environment &env) {
    for (const auto &argv : xrdb_commands(config::system_xresources, home)) {
        pid_t pid = core::spawn(argv, env);
        if (pid > 0) {
            core::wait_for(pid);
        }
    }
}

void XServerService::stop(XServer &server) {
    if (server.pid > 0) {
        kill(server.pid, SIGTERM);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config::xorg_stop_timeout_ms);
        bool exited = false;
        while (std::chrono::steady_clock::now() < deadline) {
            pid_t result = waitpid(server.pid, nullptr, WNOHANG);
            if (result == server.pid || (result < 0 && errno == ECHILD)) {
                exited = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!exited) {
            kill(server.pid, SIGKILL);
            core::wait_for(server.pid);
        }
        server.pid = 0;
    }
    std::error_code ec;
    std::filesystem::remove(server.server_auth, ec);
    std::filesystem::remove(server.user_auth, ec);
}

} // namespace service
