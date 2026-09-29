#include "service/seat_service.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern "C" {
#include <libseat.h>
}

#include "config/greet_config.h"

#include "core/log.h"
#include "core/process.h"

#include "service/auth_service.h"

namespace service {

namespace {

constexpr std::string_view session_id_key = "XDG_SESSION_ID=";

[[noreturn]] void run_holder(const std::string &tty, int vt, int ready, int hold) {
    core::reset_signal_state();
    AuthService auth;
    bool ok = auth.start(config::pam_greeter_service, config::greeter_user, tty) && auth.authenticate("").has_value();
    ok = ok && auth.put_env("XDG_SESSION_CLASS", "greeter") && auth.put_env("XDG_SESSION_TYPE", "wayland") && auth.put_env("XDG_SEAT", config::seat) && auth.put_env("XDG_VTNR", std::to_string(vt));
    ok = ok && auth.open_session();
    std::string id;
    if (ok) {
        for (const auto &entry : auth.environment()) {
            if (entry.starts_with(session_id_key)) {
                id = entry.substr(session_id_key.size());
            }
        }
    }
    std::string line = id + "\n";
    if (write(ready, line.data(), line.size()) < 0) {
        id.clear();
    }
    ::close(ready);
    char byte = 0;
    while (!id.empty()) {
        auto count = read(hold, &byte, 1);
        if (count == 0 || (count < 0 && errno != EINTR)) {
            break;
        }
    }
    auth.close_session();
    auth.end(false);
    _exit(id.empty() ? 1 : 0);
}

} // namespace

SeatService::SeatService(core::EventLoop &loop) : loop_(loop) {}

SeatService::~SeatService() {
    close();
}

bool SeatService::start_holder(const std::string &tty, int vt) {
    int ready[2];
    int hold[2];
    if (pipe2(ready, O_CLOEXEC) < 0) {
        return false;
    }
    if (pipe2(hold, O_CLOEXEC) < 0) {
        ::close(ready[0]);
        ::close(ready[1]);
        return false;
    }
    holder_ = fork();
    if (holder_ == 0) {
        ::close(ready[0]);
        ::close(hold[1]);
        run_holder(tty, vt, ready[1], hold[0]);
    }
    ::close(ready[1]);
    ::close(hold[0]);
    if (holder_ < 0) {
        holder_ = 0;
        ::close(ready[0]);
        ::close(hold[1]);
        return false;
    }
    holder_pipe_ = hold[1];
    std::string id;
    char byte = 0;
    while (read(ready[0], &byte, 1) == 1 && byte != '\n') {
        id += byte;
    }
    ::close(ready[0]);
    if (id.empty()) {
        core::error("cannot open the greeter logind session ({} as {})", config::pam_greeter_service, config::greeter_user);
        stop_holder();
        return false;
    }
    setenv("XDG_SESSION_ID", id.c_str(), 1);
    core::info("greeter logind session {} on vt{}", id, vt);
    return true;
}

void SeatService::stop_holder() {
    if (holder_pipe_ >= 0) {
        ::close(holder_pipe_);
        holder_pipe_ = -1;
    }
    if (holder_ > 0) {
        bool exited = false;
        for (int waited = 0; waited < config::holder_stop_timeout_ms && !exited; waited += 50) {
            pid_t result = waitpid(holder_, nullptr, WNOHANG);
            exited = result == holder_ || (result < 0 && errno == ECHILD);
            if (!exited) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        if (!exited) {
            kill(holder_, SIGTERM);
            core::wait_for(holder_);
        }
        holder_ = 0;
    }
    unsetenv("XDG_SESSION_ID");
}

bool SeatService::open(int vt, const std::string &tty) {
    if (!start_holder(tty, vt)) {
        return false;
    }
    setenv("LIBSEAT_BACKEND", "logind", 1);
    libseat_set_log_level(LIBSEAT_LOG_LEVEL_ERROR);
    static const libseat_seat_listener listener{&SeatService::handle_enable, &SeatService::handle_disable};
    seat_ = libseat_open_seat(&listener, this);
    if (!seat_) {
        core::error("libseat_open_seat failed: {}", std::strerror(errno));
        stop_holder();
        return false;
    }
    for (int waited = 0; !enabled_ && waited < config::seat_enable_timeout_ms; waited += 100) {
        libseat_dispatch(seat_, 100);
    }
    if (!enabled_) {
        core::error("the greeter logind session did not become active on vt{}", vt);
        close();
        return false;
    }
    loop_.add_fd(libseat_get_fd(seat_), EPOLLIN, [this](std::uint32_t) { libseat_dispatch(seat_, 0); });
    return true;
}

void SeatService::close() {
    if (seat_) {
        for (const auto &[fd, id] : devices_) {
            libseat_close_device(seat_, id);
            ::close(fd);
        }
        devices_.clear();
        loop_.remove_fd(libseat_get_fd(seat_));
        libseat_close_seat(seat_);
        seat_ = nullptr;
    }
    enabled_ = false;
    stop_holder();
}

bool SeatService::is_open() const {
    return seat_ != nullptr;
}

bool SeatService::enabled() const {
    return enabled_;
}

pid_t SeatService::holder() const {
    return holder_;
}

int SeatService::open_device(const std::string &path) {
    if (!seat_) {
        return -1;
    }
    int fd = -1;
    int id = libseat_open_device(seat_, path.c_str(), &fd);
    if (id < 0) {
        return -1;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    devices_[fd] = id;
    return fd;
}

void SeatService::close_device(int fd) {
    auto found = devices_.find(fd);
    if (found == devices_.end()) {
        ::close(fd);
        return;
    }
    libseat_close_device(seat_, found->second);
    ::close(fd);
    devices_.erase(found);
}

bool SeatService::switch_to(int vt) {
    return seat_ && libseat_switch_session(seat_, vt) == 0;
}

void SeatService::handle_enable(libseat *, void *data) {
    auto *self = static_cast<SeatService *>(data);
    self->enabled_ = true;
    if (self->on_enable) {
        self->on_enable();
    }
}

void SeatService::handle_disable(libseat *seat, void *data) {
    auto *self = static_cast<SeatService *>(data);
    self->enabled_ = false;
    if (self->on_disable) {
        self->on_disable();
    }
    libseat_disable_seat(seat);
}

} // namespace service
