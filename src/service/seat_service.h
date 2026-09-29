#pragma once

#include <functional>
#include <map>
#include <string>
#include <sys/types.h>

#include "core/event_loop.h"

struct libseat;

namespace service {

class SeatService {
  public:
    explicit SeatService(core::EventLoop &loop);
    ~SeatService();
    SeatService(const SeatService &) = delete;
    SeatService &operator=(const SeatService &) = delete;

    bool open(int vt, const std::string &tty);
    void close();
    bool is_open() const;
    bool enabled() const;
    pid_t holder() const;
    int open_device(const std::string &path);
    void close_device(int fd);
    bool switch_to(int vt);

    std::function<void()> on_enable;
    std::function<void()> on_disable;

  private:
    static void handle_enable(libseat *seat, void *data);
    static void handle_disable(libseat *seat, void *data);
    bool start_holder(const std::string &tty, int vt);
    void stop_holder();

    core::EventLoop &loop_;
    libseat *seat_ = nullptr;
    bool enabled_ = false;
    pid_t holder_ = 0;
    int holder_pipe_ = -1;
    std::map<int, int> devices_;
};

} // namespace service
