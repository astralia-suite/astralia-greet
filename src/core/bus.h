#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <systemd/sd-bus.h>

#include "core/event_loop.h"

namespace core {

struct PropertyValue {
    std::string text;
    bool flag = false;
    std::uint32_t number = 0;
};

using Properties = std::map<std::string, PropertyValue>;

class Bus {
  public:
    explicit Bus(EventLoop &loop);
    ~Bus();
    Bus(const Bus &) = delete;
    Bus &operator=(const Bus &) = delete;

    bool open();
    void close();
    bool is_open() const;
    sd_bus *handle() const;
    void process();
    bool name_has_owner(const char *name);
    bool send_async(sd_bus_message *message, sd_bus_message_handler_t handler, void *userdata, std::uint64_t timeout_us);
    bool call_async(const char *destination, const char *path, const char *interface, const char *member, sd_bus_message_handler_t handler, void *userdata, std::uint64_t timeout_us, const char *types, ...);

    static Properties read_properties(sd_bus_message *message);

    std::function<void()> on_closed;

  private:
    void arm_timeout();

    EventLoop &loop_;
    sd_bus *bus_ = nullptr;
    int timer_ = -1;
};

} // namespace core
