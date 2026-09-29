#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>

namespace core {

class EventLoop {
  public:
    using FdCallback = std::function<void(std::uint32_t)>;
    using Callback = std::function<void()>;

    EventLoop();
    ~EventLoop();
    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    bool valid() const;
    bool add_fd(int fd, std::uint32_t events, FdCallback callback);
    void remove_fd(int fd);
    int add_timer(std::chrono::milliseconds initial, std::chrono::milliseconds interval, Callback callback);
    void remove_timer(int id);
    bool add_signal(int signal, Callback callback);
    void run();
    void quit();

  private:
    void dispatch_signals();

    int epoll_fd_ = -1;
    int signal_fd_ = -1;
    bool running_ = false;
    std::map<int, std::shared_ptr<FdCallback>> fds_;
    std::map<int, Callback> signals_;
    std::set<int> timers_;
};

} // namespace core
