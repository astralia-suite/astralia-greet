#include "core/event_loop.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "core/log.h"

namespace core {

namespace {

timespec to_timespec(std::chrono::milliseconds duration) {
    timespec spec{};
    spec.tv_sec = static_cast<time_t>(duration.count() / 1000);
    spec.tv_nsec = static_cast<long>((duration.count() % 1000) * 1000000);
    return spec;
}

} // namespace

EventLoop::EventLoop() : epoll_fd_(epoll_create1(EPOLL_CLOEXEC)) {}

EventLoop::~EventLoop() {
    for (int timer : timers_) {
        ::close(timer);
    }
    if (signal_fd_ >= 0) {
        ::close(signal_fd_);
    }
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

bool EventLoop::valid() const {
    return epoll_fd_ >= 0;
}

bool EventLoop::add_fd(int fd, std::uint32_t events, FdCallback callback) {
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        error("epoll: cannot watch fd {}: {}", fd, std::strerror(errno));
        return false;
    }
    fds_[fd] = std::make_shared<FdCallback>(std::move(callback));
    return true;
}

void EventLoop::remove_fd(int fd) {
    if (fds_.erase(fd) > 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }
}

int EventLoop::add_timer(std::chrono::milliseconds initial, std::chrono::milliseconds interval, Callback callback) {
    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd < 0) {
        error("timerfd: {}", std::strerror(errno));
        return -1;
    }
    itimerspec spec{};
    spec.it_value = to_timespec(std::max(initial, std::chrono::milliseconds{1}));
    spec.it_interval = to_timespec(interval);
    timerfd_settime(fd, 0, &spec, nullptr);
    bool repeating = interval.count() > 0;
    timers_.insert(fd);
    add_fd(fd, EPOLLIN, [this, fd, repeating, callback = std::move(callback)](std::uint32_t) {
        std::uint64_t expirations = 0;
        if (::read(fd, &expirations, sizeof expirations) != sizeof expirations) {
            return;
        }
        if (!repeating) {
            remove_timer(fd);
        }
        callback();
    });
    return fd;
}

void EventLoop::remove_timer(int id) {
    if (timers_.erase(id) == 0) {
        return;
    }
    remove_fd(id);
    ::close(id);
}

bool EventLoop::add_signal(int signal, Callback callback) {
    signals_[signal] = std::move(callback);
    sigset_t mask;
    sigemptyset(&mask);
    for (const auto &[number, _] : signals_) {
        sigaddset(&mask, number);
    }
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
        return false;
    }
    bool first = signal_fd_ < 0;
    int fd = signalfd(signal_fd_, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (fd < 0) {
        error("signalfd: {}", std::strerror(errno));
        return false;
    }
    signal_fd_ = fd;
    if (first) {
        return add_fd(signal_fd_, EPOLLIN, [this](std::uint32_t) { dispatch_signals(); });
    }
    return true;
}

void EventLoop::dispatch_signals() {
    signalfd_siginfo info{};
    while (::read(signal_fd_, &info, sizeof info) == sizeof info) {
        auto found = signals_.find(static_cast<int>(info.ssi_signo));
        if (found == signals_.end()) {
            continue;
        }
        auto callback = found->second;
        callback();
    }
}

void EventLoop::run() {
    running_ = true;
    std::array<epoll_event, 16> events{};
    while (running_) {
        int count = epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), -1);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            error("epoll_wait: {}", std::strerror(errno));
            break;
        }
        for (int i = 0; i < count && running_; ++i) {
            auto found = fds_.find(events[static_cast<std::size_t>(i)].data.fd);
            if (found == fds_.end()) {
                continue;
            }
            auto callback = found->second;
            (*callback)(events[static_cast<std::size_t>(i)].events);
        }
    }
}

void EventLoop::quit() {
    running_ = false;
}

} // namespace core
